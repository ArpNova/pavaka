#pragma once

#include "parser.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

inline std::string to_nasm_db(const std::string &str) {
  std::stringstream ss;
  ss << "db ";
  if (str.empty()) {
    ss << "0\n";
    return ss.str();
  }
  bool in_quote = false;
  for (size_t i = 0; i < str.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(str[i]);
    if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
      if (!in_quote) {
        if (i > 0) ss << ", ";
        ss << "\"";
        in_quote = true;
      }
      ss << c;
    } else {
      if (in_quote) {
        ss << "\", ";
        in_quote = false;
      } else if (i > 0) {
        ss << ", ";
      }
      ss << static_cast<int>(c);
    }
  }
  if (in_quote) {
    ss << "\", ";
  } else {
    ss << ", ";
  }
  ss << "0\n";
  return ss.str();
}

class Generator {
public:
  inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {
    m_data << "newline_char db 10\n";
  }

  VarType get_expr_type(const NodeExpr *expr) const {
    struct ExprTypeVisitor {
      const Generator *gen;
      VarType operator()(const NodeTerm *term) const {
        struct TermTypeVisitor {
          const Generator *gen;
          VarType operator()(const NodeTermIntLit *) const { return VarType::Int; }
          VarType operator()(const NodeTermCharLit *) const { return VarType::Char; }
          VarType operator()(const NodeTermStringLit *) const { return VarType::String; }
          VarType operator()(const NodeTermIdent *term_ident) const {
            const auto it = std::find_if(
                gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
                  return var.name == term_ident->ident.value.value();
                });
            if (it != gen->m_vars.crend()) {
              return it->type;
            }
            return VarType::Int;
          }
          VarType operator()(const NodeTermParen *term_paren) const {
            return gen->get_expr_type(term_paren->expr);
          }
        };
        return std::visit(TermTypeVisitor{.gen = gen}, term->var);
      }
      VarType operator()(const NodeBinExpr *) const {
        return VarType::Int;
      }
    };
    return std::visit(ExprTypeVisitor{.gen = this}, expr->var);
  }

  void gen_term(const NodeTerm *term) {
    struct TermVisitor {
      Generator *gen;
      void operator()(const NodeTermIntLit *term_int_lit) const {
        gen->m_output << "    mov rax, " << term_int_lit->int_lit.value.value()
                      << "\n";
        gen->push("rax");
      }
      void operator()(const NodeTermCharLit *term_char_lit) const {
        unsigned char ch =
            static_cast<unsigned char>(term_char_lit->char_lit.value.value()[0]);
        gen->m_output << "    mov rax, " << static_cast<int>(ch) << "\n";
        gen->push("rax");
      }
      void operator()(const NodeTermStringLit *term_string_lit) const {
        const std::string label =
            "string_" + std::to_string(gen->m_string_count++);
        const std::string value = term_string_lit->string_lit.value.value();
        gen->m_data << label << " " << to_nasm_db(value);
        gen->m_output << "    mov rax, " << label << "\n";
        gen->push("rax");
      }
      void operator()(const NodeTermIdent *term_ident) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == term_ident->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << term_ident->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        std::stringstream offset;
        offset << "QWORD [rsp + "
               << (gen->m_stack_size - it->stack_loc - 1) * 8 << "]\n";
        gen->push(offset.str());
      }
      void operator()(const NodeTermParen *term_paren) const {
        gen->gen_expr(term_paren->expr);
      }
    };
    TermVisitor visitor({.gen = this});
    std::visit(visitor, term->var);
  }

  void gen_bin_expr(const NodeBinExpr *bin_expr) {
    struct BinExprVisitor {
      Generator *gen;
      void operator()(const NodeBinExprAdd *add) const {
        if (gen->get_expr_type(add->lhs) == VarType::String ||
            gen->get_expr_type(add->rhs) == VarType::String) {
          std::cerr << "Invalid operand for binary operator (cannot operate on string)" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(add->lhs);
        gen->gen_expr(add->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    add rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprMulti *multi) const {
        if (gen->get_expr_type(multi->lhs) == VarType::String ||
            gen->get_expr_type(multi->rhs) == VarType::String) {
          std::cerr << "Invalid operand for binary operator (cannot operate on string)" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(multi->lhs);
        gen->gen_expr(multi->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    mul rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprSub *sub) const {
        if (gen->get_expr_type(sub->lhs) == VarType::String ||
            gen->get_expr_type(sub->rhs) == VarType::String) {
          std::cerr << "Invalid operand for binary operator (cannot operate on string)" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(sub->lhs);
        gen->gen_expr(sub->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    sub rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprDiv *div) const {
        if (gen->get_expr_type(div->lhs) == VarType::String ||
            gen->get_expr_type(div->rhs) == VarType::String) {
          std::cerr << "Invalid operand for binary operator (cannot operate on string)" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(div->lhs);
        gen->gen_expr(div->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    xor rdx, rdx\n";
        gen->m_output << "    div rbx\n";
        gen->push("rax");
      }
    };

    BinExprVisitor visitor{.gen = this};
    std::visit(visitor, bin_expr->var);
  }

  void gen_expr(const NodeExpr *expr) {
    struct ExprVisitor {
      Generator *gen;
      void operator()(const NodeTerm *term) const { gen->gen_term(term); }
      void operator()(const NodeBinExpr *bin_expr) const {
        gen->gen_bin_expr(bin_expr);
      }
    };

    ExprVisitor visitor{.gen = this};
    std::visit(visitor, expr->var);
  }

  void gen_stmt(const NodeStmt *stmt) {
    struct StmtVisitor {
      Generator *gen;
      void operator()(const NodeStmtExit *stmt_exit) const {
        if (gen->get_expr_type(stmt_exit->expr) == VarType::String) {
          std::cerr << "Invalid expression for exit (cannot exit with string)" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(stmt_exit->expr);
        gen->m_output << "    mov rax, 60\n";
        gen->pop("rdi");
        gen->m_output << "    syscall\n";
      }
      void operator()(const NodeStmtLet *stmt_let) const {
        const size_t scope_start =
            gen->m_scopes.empty() ? 0 : gen->m_scopes.back();
        const auto it = std::find_if(
            gen->m_vars.cbegin() + scope_start, gen->m_vars.cend(),
            [&](const Var &var) {
              return var.name == stmt_let->ident.value.value();
            });
        if (it != gen->m_vars.cend()) {
          std::cerr << "Identifier already used: "
                    << stmt_let->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        VarType expr_type = gen->get_expr_type(stmt_let->expr);
        if (stmt_let->type == VarType::String && expr_type != VarType::String) {
          std::cerr << "Type mismatch: cannot assign non-string to string variable '"
                    << stmt_let->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }
        if (stmt_let->type != VarType::String && expr_type == VarType::String) {
          std::cerr << "Type mismatch: cannot assign string to non-string variable '"
                    << stmt_let->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->m_vars.push_back(Var{
            .name = stmt_let->ident.value.value(),
            .stack_loc = gen->m_stack_size,
            .type = stmt_let->type,
        });
        gen->gen_expr(stmt_let->expr);
      }
      void operator()(const NodeStmtPrint *stmt_print) const {
        VarType type = gen->get_expr_type(stmt_print->expr);
        gen->gen_expr(stmt_print->expr);
        if (type == VarType::Char) {
          gen->pop("rax");
          gen->m_output << "    push rax\n";
          gen->m_output << "    mov rax, 1\n";
          gen->m_output << "    mov rdi, 1\n";
          gen->m_output << "    mov rsi, rsp\n";
          gen->m_output << "    mov rdx, 1\n";
          gen->m_output << "    syscall\n";
          gen->m_output << "    pop rax\n";
          // Print newline
          gen->m_output << "    mov rax, 1\n";
          gen->m_output << "    mov rdi, 1\n";
          gen->m_output << "    mov rsi, newline_char\n";
          gen->m_output << "    mov rdx, 1\n";
          gen->m_output << "    syscall\n";
        } else {
          // String: rsi has pointer to null-terminated string
          gen->pop("rsi");
          std::string id = std::to_string(gen->m_label_count++);
          gen->m_output << "    mov rdx, 0\n";
          gen->m_output << ".L_strlen_" << id << ":\n";
          gen->m_output << "    cmp byte [rsi + rdx], 0\n";
          gen->m_output << "    je .L_strprint_" << id << "\n";
          gen->m_output << "    inc rdx\n";
          gen->m_output << "    jmp .L_strlen_" << id << "\n";
          gen->m_output << ".L_strprint_" << id << ":\n";
          gen->m_output << "    mov rax, 1\n";
          gen->m_output << "    mov rdi, 1\n";
          gen->m_output << "    syscall\n";
          // Print newline
          gen->m_output << "    mov rax, 1\n";
          gen->m_output << "    mov rdi, 1\n";
          gen->m_output << "    mov rsi, newline_char\n";
          gen->m_output << "    mov rdx, 1\n";
          gen->m_output << "    syscall\n";
        }
      }
      void operator()(const NodeStmtScope *stmt_scope) const {
        gen->begin_scope();
        for (const NodeStmt *stmt : stmt_scope->stmts) {
          gen->gen_stmt(stmt);
        }
        gen->end_scope();
      }
      void operator()(const NodeStmtAssign *stmt_assign) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == stmt_assign->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << stmt_assign->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        VarType expr_type = gen->get_expr_type(stmt_assign->expr);
        if (it->type == VarType::String && expr_type != VarType::String) {
          std::cerr << "Type mismatch: cannot assign non-string to string variable '"
                    << stmt_assign->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }
        if (it->type != VarType::String && expr_type == VarType::String) {
          std::cerr << "Type mismatch: cannot assign string to non-string variable '"
                    << stmt_assign->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(stmt_assign->expr);
        gen->pop("rax");
        gen->m_output << "    mov QWORD [rsp + "
                      << (gen->m_stack_size - it->stack_loc - 1) * 8
                      << "], rax\n";
      }
    };

    StmtVisitor visitor{.gen = this};
    std::visit(visitor, stmt->var);
  }

  [[nodiscard]] std::string gen_prog() {
    m_output << "section .text\n";
    m_output << "global _start\n";
    m_output << "_start:\n";

    for (const NodeStmt *stmt : m_prog.stmts) {
      gen_stmt(stmt);
    }

    m_output << "    mov rax, 60\n";
    m_output << "    mov rdi, 0\n";
    m_output << "    syscall\n";
    return "section .data\n" + m_data.str() + "\n" + m_output.str();
  }

private:
  void push(const std::string &reg) {
    m_output << "    push " << reg << "\n";
    m_stack_size++;
  }

  void pop(const std::string &reg) {
    m_output << "    pop " << reg << "\n";
    m_stack_size--;
  }

  void begin_scope() {
    m_scopes.push_back(m_vars.size());
  }

  void end_scope() {
    size_t pop_count = m_vars.size() - m_scopes.back();
    if (pop_count > 0) {
      m_output << "    add rsp, " << pop_count * 8 << "\n";
    }
    m_stack_size -= pop_count;
    for (size_t i = 0; i < pop_count; ++i) {
      m_vars.pop_back();
    }
    m_scopes.pop_back();
  }

  struct Var {
    std::string name;
    size_t stack_loc;
    VarType type;
  };

  const NodeProg m_prog;
  std::stringstream m_output;
  size_t m_stack_size = 0;
  std::vector<Var> m_vars{};
  std::vector<size_t> m_scopes{};
  std::stringstream m_data;
  size_t m_string_count = 0;
  size_t m_label_count = 0;
};