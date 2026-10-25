# Pavaka Compiler

A compiled, statically-typed systems programming language targeting x86-64 Linux. Pavaka compiles source programs (`.pavaka`) directly to native x86-64 NASM assembly and links them into standalone ELF-64 executables with no standard library or libc dependencies.

---

## Quick Example (`test.pavaka`)

```pavaka
let string message = "Hello, World!";
let int version = 1;

print($(message));
print("Pavaka Language v" $(version) " initialized.");

let status = 0;
if (status == 0) {
    print("System status: OK");
} else {
    print("System status: Error");
}

exit(status);
```

---

## Features

- **C-Compatible Memory Model**:
  - `int`: 64-bit signed integers stored on the machine stack.
  - `char`: 8-bit ASCII characters with support for escape sequences (`\n`, `\t`, `\r`, `\0`, `\'`, `\\`).
  - `string`: Null-terminated string literals in `.data` with 64-bit pointers on the stack (`char*`), supporting pointer copying and reassignment.
  - `vector<T>`: C++ `std::vector`-style dynamic arrays on the heap (`sys_brk`) for `int`, `char`, and `string`, supporting `.push_back()`, `.pop_back()`, `.size()`, element indexing (`v[i]`), and initializer lists (`{...}`).
- **Control Flow**:
  - `while (cond) { ... }` loops with full support for `break;` and `continue;` with automatic stack preservation.
  - `if`, `else if`, and `else` statements with C-style truthiness (zero is falsy, non-zero is truthy).
  - Comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`) and arithmetic (`+`, `-`, `*`, `/`) with Pratt operator precedence.
- **Lexical Scopes**:
  - Block scopes `{ ... }` with automatic stack reclamation (`add rsp, N*8`).
- **Input / Output**:
  - `print(...)`: Formatted printing supporting variables of any type via `$(var)`, string literals, characters, and auto-appended newlines.
  - `exit(...)`: Direct Linux `sys_exit` system call.
- **High-Performance Architecture**:
  - Linear bump **Arena Allocator** (4 MiB) for zero-overhead AST allocation.

---

## Documentation

- [Language Specification & Architecture Guide](docs/language-specification.md)
- [Formal Grammar Specification](docs/grammar.md)
- [Compiler Flow & Pipeline](docs/compiler-flow.md)

---

## Getting Started

### Prerequisites
- Linux OS (x86-64)
- C++20 compiler (`g++` or `clang++`)
- `cmake` (>= 3.20)
- `nasm`
- `ld` (GNU binutils)

### Building the Compiler (`pavakac`)
```bash
cmake -B build
cmake --build build
```
This produces the compiler binary `pavakac` (available in `./build/pavakac` and symlinked to `./pavakac`).

### Compiling and Executing a Program
```bash
./pavakac test.pavaka -o test
./test
echo "Exit code: $?"
```

You can also compile `test.pavaka `:
```bash
./pavakac test.pavaka -o test
./test
```
