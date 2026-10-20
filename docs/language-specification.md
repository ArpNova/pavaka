# Void Programming Language Specification & Architecture Guide

Void is a statically-typed, compiled systems programming language targeting x86-64 Linux. It compiles source files (with the `.pn` extension) directly into native x86-64 NASM assembly, producing standalone, statically-linked ELF-64 executables without external runtime libraries or libc dependencies.

---

## Table of Contents

1. [Architecture & Compilation Pipeline](#1-architecture--compilation-pipeline)
2. [Type System & Memory Model](#2-type-system--memory-model)
3. [Language Syntax & Grammar](#3-language-syntax--grammar)
4. [Statements & Control Flow](#4-statements--control-flow)
5. [Expressions & Operators](#5-expressions--operators)
6. [I/O & System Calls](#6-io--system-calls)
7. [Compiler Internals & Memory Arena](#7-compiler-internals--memory-arena)
8. [Usage & Build Instructions](#8-usage--build-instructions)
9. [Limitations & Future Roadmap](#9-limitations--future-roadmap)

---

## 1. Architecture & Compilation Pipeline

The Void compiler is built from scratch in modern C++20 with zero third-party library dependencies.

```
+---------------------------------------------------------------------------------------+
|                                    Void Pipeline                                      |
+---------------------------------------------------------------------------------------+
|  Source (.pn)                                                                         |
|       |                                                                               |
|       v                                                                               |
|  Tokenizer (tokenization.hpp) --------> Converts raw text into Token stream           |
|       |                                                                               |
|       v                                                                               |
|  Parser (parser.hpp) -----------------> Recursive-descent Pratt parser               |
|       |  (uses arena.hpp for AST)       Constructs NodeProg Abstract Syntax Tree      |
|       v                                                                               |
|  Generator (generation.hpp) ----------> Walks AST; tracks variables & stack offsets; |
|       |                                 Generates out.asm (x86-64 NASM assembly)      |
|       v                                                                               |
|  NASM Assembler ----------------------> nasm -felf64 out.asm -> out.o                 |
|       |                                                                               |
|       v                                                                               |
|  GNU Linker (ld) ---------------------> ld -o out out.o -> native ELF-64 binary      |
|       |                                                                               |
|       v                                                                               |
|  Executable (./out)                                                                   |
+---------------------------------------------------------------------------------------+
```

### Compiler Phases

1. **Tokenization (`src/tokenization.hpp`)**:
   Reads the entire source into memory and performs lexical analysis. Identifies keywords (`let`, `string`, `char`, `int`, `if`, `else`, `print`, `exit`), multi-character operators (`==`, `!=`, `<=`, `>=`), single-character operators, identifiers, integer literals, escape-processed character literals (`'a'`, `'\n'`), and string literals (`"..."`).

2. **Parsing & AST Construction (`src/parser.hpp`)**:
   Uses recursive descent combined with Pratt parsing (precedence climbing) for binary expressions. All AST nodes are allocated out of a dedicated 4 MiB memory arena (`ArenaAllocator` in `src/arena.hpp`), guaranteeing fast bump-pointer allocation and zero-overhead teardown.

3. **Code Generation (`src/generation.hpp`)**:
   Traverses AST nodes using `std::visit` and visitor structures. Maintains a compile-time variable stack map and scope boundaries. Emits NASM assembly in two sections:
   - `section .data`: Static null-terminated string literals and character constants.
   - `section .text`: The `_start` symbol, control flow jumps, expression evaluations, and raw Linux system calls (`syscall`).

4. **Assembly and Linking (`src/main.cpp`)**:
   Invokes `nasm -felf64 out.asm` to assemble the output into an object file (`out.o`), then executes `ld -o out out.o` to link the final native Linux executable.

---

## 2. Type System & Memory Model

Void adopts the **C language memory and type philosophy**:

| Type | Size | Storage Representation | C Equivalent |
| :--- | :--- | :--- | :--- |
| `int` | 64 bits (8 bytes) | 64-bit two's complement integer stored on the hardware stack | `int64_t` / `long long` |
| `char` | 8 bits (1 byte) | 8-bit ASCII value, stored in 64-bit aligned stack slots | `char` |
| `string` | 64 bits (8 bytes) | 64-bit memory address (`char*`) pointing to static `.data` segment | `const char*` |

### String Memory Layout (C-Style Null-Terminated Strings)
String literals in Void have static storage duration and are placed in the read-only `.data` segment as null-terminated byte sequences:
```nasm
section .data
string_0 db "Hello, World!", 0
```
When a string variable is declared:
```pn
let string s = "Hello, World!";
```
The variable `s` does **not** store the string text directly on the stack. Instead, its stack slot stores the **64-bit memory address** pointing to `string_0`:
```nasm
    mov rax, string_0
    push rax
```

#### Pointer Copy vs. Reassignment
- **Copying (`let string b = a;`)**: Copies the 64-bit memory address from `a` to `b` (shallow copy). Both variables now reference the same memory in `.data`.
- **Reassignment (`a = "new text";`)**: Creates a new null-terminated entry in `.data` (e.g., `string_1 db "new text", 0`) and updates the pointer in `a`'s stack slot. Variable `b` continues to point to `string_0`.

### Character Representation
Characters are single-byte ASCII integers. When evaluated in expressions, they promote to integers, allowing arithmetic operations directly in C-style fashion:
```pn
let char letter = 'A';
let char next = letter + 1; // Evaluates to 'B' (ASCII 66)
```

### Stack Frame & Scoping
Void maintains an 8-byte aligned hardware stack (`rsp`). Variables are referenced relative to the current stack pointer:
$$\text{Offset} = (\text{stack\_size} - \text{stack\_loc} - 1) \times 8$$
When exiting a lexical scope `{ ... }`, the stack pointer is restored with:
```nasm
    add rsp, pop_count * 8
```
All local variables allocated inside that block are immediately reclaimed, while static data in `.data` remains allocated for the entire lifetime of the process.

---

## 3. Language Syntax & Grammar

### Formal Grammar (Extended Backus-Naur Form)

$$
\begin{align}
    [\text{Prog}] &\to [\text{Stmt}]^* \\
    [\text{Stmt}] &\to
    \begin{cases}
        \text{exit}([\text{Expr}]); \\
        \text{print}([\text{Expr}]^+); \\
        \text{let}\space\text{string}\space\text{ident} = [\text{Expr}]; \\
        \text{let}\space\text{char}\space\text{ident} = [\text{Expr}]; \\
        \text{let}\space\text{int}?\space\text{ident} = [\text{Expr}]; \\
        \text{ident} = [\text{Expr}]; \\
        \text{if}\space([\text{Expr}])[\text{Scope}][\text{IfPred}]? \\
        [\text{Scope}]
    \end{cases} \\
    [\text{IfPred}] &\to
    \begin{cases}
        \text{else}\space\text{if}\space([\text{Expr}])[\text{Scope}][\text{IfPred}]? \\
        \text{else}\space[\text{Scope}]
    \end{cases} \\
    [\text{Scope}] &\to \{[\text{Stmt}]^*\} \\
    [\text{Expr}] &\to
    \begin{cases}
        [\text{Term}] \\
        [\text{BinExpr}]
    \end{cases} \\
    [\text{BinExpr}] &\to
    \begin{cases}
        [\text{Expr}] * [\text{Expr}] & \text{prec} = 4 \\
        [\text{Expr}] / [\text{Expr}] & \text{prec} = 4 \\
        [\text{Expr}] + [\text{Expr}] & \text{prec} = 3 \\
        [\text{Expr}] - [\text{Expr}] & \text{prec} = 3 \\
        [\text{Expr}] < [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] <= [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] > [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] >= [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] == [\text{Expr}] & \text{prec} = 1 \\
        [\text{Expr}] != [\text{Expr}] & \text{prec} = 1 \\
    \end{cases} \\ 
    [\text{Term}] &\to
    \begin{cases}
        \text{int\_lit} \\
        \text{string\_lit} \\
        \text{char\_lit} \\
        \text{ident} \\
        \$(\text{ident}) \\
        ([\text{Expr}])
    \end{cases}
\end{align}
$$

---

## 4. Statements & Control Flow

### Variable Declarations
Variables are declared using the `let` keyword:

```pn
let x = 42;                 // Inferred int (default)
let int counter = 100;      // Explicit int
let char symbol = 'Z';      // Character literal
let string message = "Hi";  // String literal (char*)
```

### Variable Reassignment
Existing variables can be reassigned using `=`:
```pn
let x = 10;
x = 20;

let string status = "pending";
status = "completed";
```
Type safety is strictly enforced: attempting to assign a string to an integer variable or an integer to a string variable causes a compile-time `Type mismatch` error.

### Lexical Scopes
Code enclosed in `{ ... }` creates a distinct lexical scope:
```pn
let outer = 1;
{
    let inner = 2;
    let outer = 99; // Shadowed inside block
    print($(outer)); // Outputs: 99
}
print($(outer));     // Outputs: 1; 'inner' is deallocated from stack
```

### Conditional Statements (`if`, `else if`, `else`)
Conditionals follow C semantics: any non-zero value is truthy, while `0` is falsy.

```pn
let score = 85;

if (score >= 90) {
    print("Grade: A");
} else if (score >= 80) {
    print("Grade: B");
} else if (score >= 70) {
    print("Grade: C");
} else {
    print("Grade: F");
}
```

Nested conditionals are fully supported:
```pn
if (x > 0) {
    if (x < 100) {
        print("x is within (0, 100)");
    }
}
```

---

## 5. Expressions & Operators

### Operator Precedence

| Precedence | Operators | Description | Associativity |
| :---: | :---: | :--- | :---: |
| 4 (Highest) | `*`, `/` | Multiplication, Integer Division | Left |
| 3 | `+`, `-` | Addition, Subtraction | Left |
| 2 | `<`, `<=`, `>`, `>=` | Relational Comparisons | Left |
| 1 (Lowest) | `==`, `!=` | Equality and Inequality | Left |

### Comparison Semantics
All comparison operators evaluate to either `1` (true) or `0` (false) as a 64-bit integer on the stack:
```pn
let is_equal = (5 == 5);  // Evaluates to 1
let is_less = (10 < 2);   // Evaluates to 0
```
Under the hood, comparisons are emitted with x86-64 `cmp` followed by conditional byte sets (`sete`, `setne`, `setl`, `setle`, `setg`, `setge`) and zero-extension (`movzx`).

---

## 6. I/O & System Calls

Void does not link against the C standard library (`libc`). All input and output operations invoke Linux kernel system calls directly.

### The `print` Statement
`print(...)` prints one or more expressions to standard output (`stdout`), followed automatically by a newline character.

#### Multi-Argument Printing & Variable Interpolation (`$(var)`)
Outside quotation marks, variable references can be written using `$(variable_name)`:
```pn
let int age = 21;
let char grade = 'A';
let string name = "Alice";

print("User: " $(name) ", Age: " $(age) ", Grade: " $(grade));
```
**Output:**
```
User: Alice, Age: 21, Grade: A
```

#### How Each Type Is Printed
1. **`int`**: Converted to decimal ASCII characters using an in-place stack buffer (handling `0`, positive numbers, and negative numbers with leading `-`) and written via `sys_write` (syscall 1).
2. **`char`**: The single ASCII byte is pushed to the stack and written directly to `stdout`.
3. **`string`**: The pointer in `rsi` is traversed with a null-terminator loop to determine length (`strlen`), then written via `sys_write`.

### The `exit` Statement
Exits the process immediately with a given integer exit code using Linux `sys_exit` (syscall 60):
```pn
exit(0);       // Success
exit(x);       // Exit with value of x
exit('A');     // Exits with code 65
```

---

## 7. Compiler Internals & Memory Arena

### Arena Allocator (`src/arena.hpp`)
In conventional compilers, recursive AST node allocations produce millions of small heap allocations (`new` / `malloc`), leading to heap fragmentation and slow traversal.

Void implements a linear bump **Arena Allocator**:
```cpp
class ArenaAllocator {
public:
  inline explicit ArenaAllocator(size_t bytes) : m_size(bytes) {
    m_buffer = static_cast<std::byte *>(malloc(m_size));
    m_offset = m_buffer;
  }
  template <typename T> inline T *alloc() {
    std::byte *user_ptr = m_offset;
    m_offset += sizeof(T);
    return reinterpret_cast<T *>(user_ptr);
  }
  ~ArenaAllocator() { free(m_buffer); }
};
```
- A single 4 MiB block is allocated upon parser initialization.
- Every AST node creation (`NodeTerm`, `NodeExpr`, `NodeStmt`, etc.) costs a single pointer addition.
- Deallocating the entire syntax tree requires one single `free()` when the parser goes out of scope.

---

## 8. Usage & Build Instructions

### Prerequisites
- Linux OS (x86-64)
- C++20 compatible compiler (`g++` $\ge$ 11 or `clang++` $\ge$ 13)
- CMake $\ge$ 3.20
- NASM (Netwide Assembler)
- GNU Linker (`ld`, part of `binutils`)

Install prerequisites on Debian/Ubuntu:
```bash
sudo apt update
sudo apt install build-essential cmake nasm
```

### Compiling the Void Compiler
```bash
cmake -B build
cmake --build build
```
This generates the compiler binary at `./build/void`.

### Compiling and Running a Program
```bash
# Compile source to executable
./build/void test.pn

# Run the produced executable
./out

# Check exit code
echo "Exit code: $?"
```

### Inspecting Generated Assembly
The compiler saves intermediate assembly to `out.asm`:
```bash
cat out.asm
```

---

## 9. Limitations & Future Roadmap

| Feature | Current Status | Planned Implementation |
| :--- | :--- | :--- |
| **Data Types** | `int`, `char`, `string` | `bool`, `float`, fixed-size arrays |
| **Loops** | Not supported | `while (cond) { ... }`, `for (...)` loops |
| **Functions** | Single entry point (`_start`) | User-defined functions with System V AMD64 ABI call frames (`call`/`ret`) |
| **Heap Memory** | Static (`.data`) & Stack only | Dynamic heap allocations (`malloc`, `free`) |
| **Floating Point** | Not supported | SSE/AVX registers (`xmm0`-`xmm7`) |
| **Comments** | Lexer does not parse `//` or `/* */` | Inline `//` and block `/* */` comments |
| **String Concatenation** | Not supported (pointer comparison only) | Built-in string concatenation & helper functions |
| **Target Architecture** | x86-64 Linux only | ARM64 (aarch64) and macOS/Mach-O support |
