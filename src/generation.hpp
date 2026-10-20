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
      void operator()(const NodeTermParen *term_paren)const{
        gen->gen_expr(term_paren->expr);
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

      void operator()(const NodeBinExprSub* sub) const {
        gen->gen_expr(sub->lhs);
        gen->gen_expr(sub->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    sub rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprDiv* div) const {
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
        gen->m_vars.push_back(Var{
            .name = stmt_let->ident.value.value(),
            .stack_loc = gen->m_stack_size,
        });
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
  };

  const NodeProg m_prog;
  std::stringstream m_output;
  size_t m_stack_size = 0;
  std::vector<Var> m_vars{};
  std::vector<size_t> m_scopes{};
  std::stringstream m_data;
  size_t m_string_count = 0;
};