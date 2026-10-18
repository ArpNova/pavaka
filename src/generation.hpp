#pragma once

#include "parser.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

class Generator {
public:
  inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {}

  void gen_term(const NodeTerm *term) {
    struct TermVisitor {
      Generator *gen;
      void operator()(const NodeTermIntLit *term_int_lit) const {
        gen->m_output << "    mov rax, " << term_int_lit->int_lit.value.value()
                      << "\n";
        gen->push("rax");
      }
      void operator()(const NodeTermIdent *term_ident) const {
        if (gen->m_vars.find(term_ident->ident.value.value()) ==
            gen->m_vars.end()) {
          std::cerr << "Undeclared identifier: "
                    << term_ident->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        const auto &var = gen->m_vars.at(term_ident->ident.value.value());
        std::stringstream offset;
        offset << "QWORD [rsp + " << (gen->m_stack_size - var.stack_loc - 1) * 8
               << "]\n";
        gen->push(offset.str());
      }
    };
    TermVisitor visitor({.gen = this});
    std::visit(visitor, term->var);
  }

  void gen_bin_expr(const NodeBinExpr *bin_expr) {
    struct BinExprVisitor {
      Generator *gen;
      void operator()(const NodeBinExprAdd* add) const{
        gen->gen_expr(add->lhs);
        gen->gen_expr(add->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    add rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprMulti* multi) const {
        gen->gen_expr(multi->lhs);
        gen->gen_expr(multi->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    mul rbx\n";
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
        gen->gen_expr(stmt_exit->expr);
        gen->m_output << "    mov rax, 60\n";
        gen->pop("rdi");
        gen->m_output << "    syscall\n";
      }
      void operator()(const NodeStmtLet *stmt_let) const {
        if (gen->m_vars.find(stmt_let->ident.value.value()) !=
            gen->m_vars.end()) {
          std::cerr << "Identifier already used: "
                    << stmt_let->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->m_vars.insert({stmt_let->ident.value.value(),
                            Var{.stack_loc = gen->m_stack_size}});
        gen->gen_expr(stmt_let->expr);
      }
      void operator()(const NodeStmtPrint *stmt_print) const {
        const std::string label =
            "string_" + std::to_string(gen->m_string_count++);
        const std::string value = stmt_print->string_lit.value.value();

        gen->m_data << label << " db \"" << value << "\", 10\n"
                    << label << "_length equ $ - " << label << "\n";

        gen->m_output << "    mov rax, 1\n"
                      << "    mov rdi, 1\n"
                      << "    mov rsi, " << label << "\n"
                      << "    mov rdx, " << label << "_length\n"
                      << "    syscall\n";
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

  struct Var {
    size_t stack_loc;
  };

  const NodeProg m_prog;
  std::stringstream m_output;
  size_t m_stack_size = 0;
  std::unordered_map<std::string, Var> m_vars{};
  std::stringstream m_data;
  size_t m_string_count = 0;
};