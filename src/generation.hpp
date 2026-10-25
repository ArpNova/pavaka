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
  struct Var {
    std::string name;
    size_t stack_loc;
    VarType type;
  };

  struct Scope {
    size_t var_idx;
    size_t stack_size;
  };

  struct LoopContext {
    std::string start_label;
    std::string end_label;
    size_t stack_size;
  };

  std::string get_vector_data_addr(const Var &var) const {
    return "QWORD [rsp + " + std::to_string((m_stack_size - var.stack_loc - 1) * 8) + "]";
  }

  std::string get_vector_size_addr(const Var &var) const {
    return "QWORD [rsp + " + std::to_string((m_stack_size - (var.stack_loc + 1) - 1) * 8) + "]";
  }

  std::string get_vector_cap_addr(const Var &var) const {
    return "QWORD [rsp + " + std::to_string((m_stack_size - (var.stack_loc + 2) - 1) * 8) + "]";
  }

  inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {
    m_data << "newline_char db 10\n";
    m_data << "__heap_current dq 0\n";
    m_data << "__str_lbracket db \"[\", 0\n";
    m_data << "__str_rbracket db \"]\", 0\n";
    m_data << "__str_comma_space db \", \", 0\n";
    m_data << "__str_squote db \"'\", 0\n";
    m_data << "__str_dquote db '\"', 0\n";
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
            std::cerr << "Undeclared identifier: " << term_ident->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
          }
          VarType operator()(const NodeTermParen *term_paren) const {
            return gen->get_expr_type(term_paren->expr);
          }
          VarType operator()(const NodeTermVectorAccess *term_acc) const {
            const auto it = std::find_if(
                gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
                  return var.name == term_acc->ident.value.value();
                });
            if (it == gen->m_vars.crend()) {
              std::cerr << "Undeclared identifier: " << term_acc->ident.value.value() << std::endl;
              exit(EXIT_FAILURE);
            }
            if (!is_vector_type(it->type)) {
              std::cerr << "Cannot index non-vector variable: " << term_acc->ident.value.value() << std::endl;
              exit(EXIT_FAILURE);
            }
            if (gen->get_expr_type(term_acc->index) != VarType::Int) {
              std::cerr << "Vector index must be an integer" << std::endl;
              exit(EXIT_FAILURE);
            }
            return vector_elem_type(it->type);
          }
          VarType operator()(const NodeTermVectorSize *term_size) const {
            const auto it = std::find_if(
                gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
                  return var.name == term_size->ident.value.value();
                });
            if (it == gen->m_vars.crend()) {
              std::cerr << "Undeclared identifier: " << term_size->ident.value.value() << std::endl;
              exit(EXIT_FAILURE);
            }
            if (!is_vector_type(it->type)) {
              std::cerr << "Cannot call size() on non-vector variable: " << term_size->ident.value.value() << std::endl;
              exit(EXIT_FAILURE);
            }
            return VarType::Int;
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
      void operator()(const NodeTermVectorAccess *term_acc) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == term_acc->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << term_acc->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(term_acc->index);
        gen->pop("rcx"); // rcx = index
        gen->m_output << "    mov rbx, " << gen->get_vector_data_addr(*it) << "\n";
        if (it->type == VarType::VectorChar) {
          gen->m_output << "    movzx rax, byte [rbx + rcx]\n";
        } else {
          gen->m_output << "    mov rax, qword [rbx + rcx * 8]\n";
        }
        gen->push("rax");
      }
      void operator()(const NodeTermVectorSize *term_size) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == term_size->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << term_size->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->m_output << "    mov rax, " << gen->get_vector_size_addr(*it) << "\n";
        gen->push("rax");
      }
    };
    TermVisitor visitor({.gen = this});
    std::visit(visitor, term->var);
  }

  void gen_bin_expr(const NodeBinExpr *bin_expr) {
    struct BinExprVisitor {
      Generator *gen;
      void check_operands(const NodeExpr *lhs, const NodeExpr *rhs) const {
        VarType lt = gen->get_expr_type(lhs);
        VarType rt = gen->get_expr_type(rhs);
        if (lt == VarType::String || rt == VarType::String ||
            is_vector_type(lt) || is_vector_type(rt)) {
          std::cerr << "Invalid operand for binary operator" << std::endl;
          exit(EXIT_FAILURE);
        }
      }

      void operator()(const NodeBinExprAdd *add) const {
        check_operands(add->lhs, add->rhs);
        gen->gen_expr(add->lhs);
        gen->gen_expr(add->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    add rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprMulti *multi) const {
        check_operands(multi->lhs, multi->rhs);
        gen->gen_expr(multi->lhs);
        gen->gen_expr(multi->rhs);
        gen->pop("rax");
        gen->pop("rbx");
        gen->m_output << "    mul rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprSub *sub) const {
        check_operands(sub->lhs, sub->rhs);
        gen->gen_expr(sub->lhs);
        gen->gen_expr(sub->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    sub rax, rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprDiv *div) const {
        check_operands(div->lhs, div->rhs);
        gen->gen_expr(div->lhs);
        gen->gen_expr(div->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    xor rdx, rdx\n";
        gen->m_output << "    div rbx\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprEq *eq) const {
        check_operands(eq->lhs, eq->rhs);
        gen->gen_expr(eq->lhs);
        gen->gen_expr(eq->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    sete al\n";
        gen->m_output << "    movzx rax, al\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprNotEq *node_not_eq) const {
        check_operands(node_not_eq->lhs, node_not_eq->rhs);
        gen->gen_expr(node_not_eq->lhs);
        gen->gen_expr(node_not_eq->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    setne al\n";
        gen->m_output << "    movzx rax, al\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprLess *less) const {
        check_operands(less->lhs, less->rhs);
        gen->gen_expr(less->lhs);
        gen->gen_expr(less->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    setl al\n";
        gen->m_output << "    movzx rax, al\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprLessEq *less_eq) const {
        check_operands(less_eq->lhs, less_eq->rhs);
        gen->gen_expr(less_eq->lhs);
        gen->gen_expr(less_eq->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    setle al\n";
        gen->m_output << "    movzx rax, al\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprGreater *greater) const {
        check_operands(greater->lhs, greater->rhs);
        gen->gen_expr(greater->lhs);
        gen->gen_expr(greater->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    setg al\n";
        gen->m_output << "    movzx rax, al\n";
        gen->push("rax");
      }

      void operator()(const NodeBinExprGreaterEq *greater_eq) const {
        check_operands(greater_eq->lhs, greater_eq->rhs);
        gen->gen_expr(greater_eq->lhs);
        gen->gen_expr(greater_eq->rhs);
        gen->pop("rbx");
        gen->pop("rax");
        gen->m_output << "    cmp rax, rbx\n";
        gen->m_output << "    setge al\n";
        gen->m_output << "    movzx rax, al\n";
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

  void emit_print_int_from_rax() {
    std::string id = std::to_string(m_label_count++);
    m_output << "    sub rsp, 32\n";
    m_output << "    lea rsi, [rsp + 31]\n";
    m_output << "    mov byte [rsi], 0\n";
    m_output << "    mov rcx, 0\n";
    m_output << "    cmp rax, 0\n";
    m_output << "    jne .L_check_neg_" << id << "\n";
    m_output << "    dec rsi\n";
    m_output << "    mov byte [rsi], '0'\n";
    m_output << "    inc rcx\n";
    m_output << "    jmp .L_print_int_" << id << "\n";
    m_output << ".L_check_neg_" << id << ":\n";
    m_output << "    mov r8, 0\n";
    m_output << "    cmp rax, 0\n";
    m_output << "    jge .L_div_loop_" << id << "\n";
    m_output << "    mov r8, 1\n";
    m_output << "    neg rax\n";
    m_output << ".L_div_loop_" << id << ":\n";
    m_output << "    cmp rax, 0\n";
    m_output << "    je .L_done_div_" << id << "\n";
    m_output << "    xor rdx, rdx\n";
    m_output << "    mov rbx, 10\n";
    m_output << "    div rbx\n";
    m_output << "    add rdx, '0'\n";
    m_output << "    dec rsi\n";
    m_output << "    mov byte [rsi], dl\n";
    m_output << "    inc rcx\n";
    m_output << "    jmp .L_div_loop_" << id << "\n";
    m_output << ".L_done_div_" << id << ":\n";
    m_output << "    cmp r8, 1\n";
    m_output << "    jne .L_print_int_" << id << "\n";
    m_output << "    dec rsi\n";
    m_output << "    mov byte [rsi], '-'\n";
    m_output << "    inc rcx\n";
    m_output << ".L_print_int_" << id << ":\n";
    m_output << "    mov rax, 1\n";
    m_output << "    mov rdi, 1\n";
    m_output << "    mov rdx, rcx\n";
    m_output << "    syscall\n";
    m_output << "    add rsp, 32\n";
  }

  void emit_print_string_from_rsi() {
    std::string id = std::to_string(m_label_count++);
    m_output << "    mov rdx, 0\n";
    m_output << ".L_strlen_" << id << ":\n";
    m_output << "    cmp byte [rsi + rdx], 0\n";
    m_output << "    je .L_strprint_" << id << "\n";
    m_output << "    inc rdx\n";
    m_output << "    jmp .L_strlen_" << id << "\n";
    m_output << ".L_strprint_" << id << ":\n";
    m_output << "    mov rax, 1\n";
    m_output << "    mov rdi, 1\n";
    m_output << "    syscall\n";
  }

  void gen_stmt(const NodeStmt *stmt) {
    struct StmtVisitor {
      Generator *gen;
      void operator()(const NodeStmtExit *stmt_exit) const {
        VarType t = gen->get_expr_type(stmt_exit->expr);
        if (t == VarType::String || is_vector_type(t)) {
          std::cerr << "Invalid expression for exit" << std::endl;
          exit(EXIT_FAILURE);
        }
        gen->gen_expr(stmt_exit->expr);
        gen->m_output << "    mov rax, 60\n";
        gen->pop("rdi");
        gen->m_output << "    syscall\n";
      }

      void operator()(const NodeStmtLet *stmt_let) const {
        const size_t scope_start =
            gen->m_scopes.empty() ? 0 : gen->m_scopes.back().var_idx;
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

      void operator()(const NodeStmtLetVector *stmt_vec) const {
        const size_t scope_start =
            gen->m_scopes.empty() ? 0 : gen->m_scopes.back().var_idx;
        const auto it = std::find_if(
            gen->m_vars.cbegin() + scope_start, gen->m_vars.cend(),
            [&](const Var &var) {
              return var.name == stmt_vec->ident.value.value();
            });
        if (it != gen->m_vars.cend()) {
          std::cerr << "Identifier already used: "
                    << stmt_vec->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }

        VarType elem_type = vector_elem_type(stmt_vec->type);
        for (const NodeExpr *elem : stmt_vec->init_elements) {
          if (gen->get_expr_type(elem) != elem_type) {
            std::cerr << "Type mismatch in vector initializer list for '"
                      << stmt_vec->ident.value.value() << "'" << std::endl;
            exit(EXIT_FAILURE);
          }
        }

        size_t init_count = stmt_vec->init_elements.size();
        size_t initial_cap = std::max((size_t)4, init_count);
        size_t elem_size = (stmt_vec->type == VarType::VectorChar) ? 1 : 8;

        // Allocate buffer on heap
        gen->m_output << "    mov rdi, " << (initial_cap * elem_size) << "\n";
        gen->m_output << "    call __pavaka_alloc\n";

        if (init_count > 0) {
          gen->m_output << "    mov r12, rax\n"; // r12 = buffer
          for (size_t i = 0; i < init_count; ++i) {
            gen->gen_expr(stmt_vec->init_elements[i]);
            gen->pop("rax");
            if (elem_size == 1) {
              gen->m_output << "    mov byte [r12 + " << i << "], al\n";
            } else {
              gen->m_output << "    mov qword [r12 + " << (i * 8) << "], rax\n";
            }
          }
          gen->m_output << "    push r12\n";
          gen->m_stack_size++;
          gen->m_output << "    push " << init_count << "\n";
          gen->m_stack_size++;
          gen->m_output << "    push " << initial_cap << "\n";
          gen->m_stack_size++;
        } else {
          gen->push("rax"); // data_ptr
          gen->m_output << "    push 0\n"; // size
          gen->m_stack_size++;
          gen->m_output << "    push " << initial_cap << "\n"; // capacity
          gen->m_stack_size++;
        }

        gen->m_vars.push_back(Var{
            .name = stmt_vec->ident.value.value(),
            .stack_loc = gen->m_stack_size - 3,
            .type = stmt_vec->type,
        });
      }

      void operator()(const NodeStmtVectorPush *stmt_push) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == stmt_push->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << stmt_push->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        if (!is_vector_type(it->type)) {
          std::cerr << "Cannot push_back on non-vector variable: "
                    << stmt_push->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        VarType elem_type = vector_elem_type(it->type);
        if (gen->get_expr_type(stmt_push->expr) != elem_type) {
          std::cerr << "Type mismatch in push_back for variable '"
                    << stmt_push->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }

        gen->gen_expr(stmt_push->expr);
        gen->pop("r13"); // r13 holds value to push

        std::string id = std::to_string(gen->m_label_count++);
        std::string data_addr = gen->get_vector_data_addr(*it);
        std::string size_addr = gen->get_vector_size_addr(*it);
        std::string cap_addr = gen->get_vector_cap_addr(*it);
        size_t elem_size = (it->type == VarType::VectorChar) ? 1 : 8;

        gen->m_output << "    mov rbx, " << size_addr << "\n";
        gen->m_output << "    mov rcx, " << cap_addr << "\n";
        gen->m_output << "    cmp rbx, rcx\n";
        gen->m_output << "    jl .L_vec_has_cap_" << id << "\n";
        // Reallocate
        gen->m_output << "    cmp rcx, 0\n";
        gen->m_output << "    jne .L_vec_double_" << id << "\n";
        gen->m_output << "    mov rcx, 4\n";
        gen->m_output << "    jmp .L_vec_alloc_" << id << "\n";
        gen->m_output << ".L_vec_double_" << id << ":\n";
        gen->m_output << "    shl rcx, 1\n";
        gen->m_output << ".L_vec_alloc_" << id << ":\n";
        gen->m_output << "    mov " << cap_addr << ", rcx\n";
        if (elem_size == 1) {
          gen->m_output << "    mov rdi, rcx\n";
        } else {
          gen->m_output << "    lea rdi, [rcx * 8]\n";
        }
        gen->m_output << "    call __pavaka_alloc\n";
        gen->m_output << "    mov r12, rax\n"; // new buffer
        gen->m_output << "    mov rsi, " << data_addr << "\n";
        gen->m_output << "    mov rdi, r12\n";
        gen->m_output << "    mov rcx, " << size_addr << "\n";
        if (elem_size == 8) {
          gen->m_output << "    shl rcx, 3\n";
        }
        gen->m_output << "    rep movsb\n";
        gen->m_output << "    mov " << data_addr << ", r12\n";
        gen->m_output << ".L_vec_has_cap_" << id << ":\n";
        gen->m_output << "    mov rbx, " << data_addr << "\n";
        gen->m_output << "    mov rcx, " << size_addr << "\n";
        if (elem_size == 1) {
          gen->m_output << "    mov byte [rbx + rcx], r13b\n";
        } else {
          gen->m_output << "    mov qword [rbx + rcx * 8], r13\n";
        }
        gen->m_output << "    inc rcx\n";
        gen->m_output << "    mov " << size_addr << ", rcx\n";
      }

      void operator()(const NodeStmtVectorPop *stmt_pop) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == stmt_pop->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << stmt_pop->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        if (!is_vector_type(it->type)) {
          std::cerr << "Cannot pop_back on non-vector variable: "
                    << stmt_pop->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        std::string id = std::to_string(gen->m_label_count++);
        std::string size_addr = gen->get_vector_size_addr(*it);
        gen->m_output << "    mov rcx, " << size_addr << "\n";
        gen->m_output << "    cmp rcx, 0\n";
        gen->m_output << "    jle .L_pop_done_" << id << "\n";
        gen->m_output << "    dec rcx\n";
        gen->m_output << "    mov " << size_addr << ", rcx\n";
        gen->m_output << ".L_pop_done_" << id << ":\n";
      }

      void operator()(const NodeStmtVectorAssign *stmt_assign) const {
        const auto it = std::find_if(
            gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &var) {
              return var.name == stmt_assign->ident.value.value();
            });
        if (it == gen->m_vars.crend()) {
          std::cerr << "Undeclared identifier: "
                    << stmt_assign->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        if (!is_vector_type(it->type)) {
          std::cerr << "Cannot index assign non-vector variable: "
                    << stmt_assign->ident.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
        if (gen->get_expr_type(stmt_assign->index) != VarType::Int) {
          std::cerr << "Vector index must be an integer" << std::endl;
          exit(EXIT_FAILURE);
        }
        VarType elem_type = vector_elem_type(it->type);
        if (gen->get_expr_type(stmt_assign->value) != elem_type) {
          std::cerr << "Type mismatch in index assignment for '"
                    << stmt_assign->ident.value.value() << "'" << std::endl;
          exit(EXIT_FAILURE);
        }

        gen->gen_expr(stmt_assign->index); // pushes index
        gen->gen_expr(stmt_assign->value); // pushes value
        gen->pop("rax"); // value
        gen->pop("rcx"); // index
        gen->m_output << "    mov rbx, " << gen->get_vector_data_addr(*it) << "\n";
        if (it->type == VarType::VectorChar) {
          gen->m_output << "    mov byte [rbx + rcx], al\n";
        } else {
          gen->m_output << "    mov qword [rbx + rcx * 8], rax\n";
        }
      }

      void operator()(const NodeStmtPrint *stmt_print) const {
        for (const NodeExpr *expr : stmt_print->exprs) {
          VarType type = gen->get_expr_type(expr);
          if (type == VarType::Int) {
            gen->gen_expr(expr);
            gen->pop("rax");
            gen->emit_print_int_from_rax();
          } else if (type == VarType::Char) {
            gen->gen_expr(expr);
            gen->pop("rax");
            gen->m_output << "    push rax\n";
            gen->m_output << "    mov rax, 1\n";
            gen->m_output << "    mov rdi, 1\n";
            gen->m_output << "    mov rsi, rsp\n";
            gen->m_output << "    mov rdx, 1\n";
            gen->m_output << "    syscall\n";
            gen->m_output << "    pop rax\n";
          } else if (type == VarType::String) {
            gen->gen_expr(expr);
            gen->pop("rsi");
            gen->emit_print_string_from_rsi();
          } else if (is_vector_type(type)) {
            // Find variable to print vector elements
            std::string var_name;
            const NodeExpr *curr_expr = expr;
            while (curr_expr && std::holds_alternative<NodeTerm *>(curr_expr->var)) {
              auto *term = std::get<NodeTerm *>(curr_expr->var);
              if (std::holds_alternative<NodeTermIdent *>(term->var)) {
                var_name = std::get<NodeTermIdent *>(term->var)->ident.value.value();
                break;
              } else if (std::holds_alternative<NodeTermParen *>(term->var)) {
                curr_expr = std::get<NodeTermParen *>(term->var)->expr;
              } else {
                break;
              }
            }

            const auto it = std::find_if(
                gen->m_vars.crbegin(), gen->m_vars.crend(), [&](const Var &v) {
                  return v.name == var_name;
                });
            if (it != gen->m_vars.crend()) {
              std::string id = std::to_string(gen->m_label_count++);
              std::string data_addr = gen->get_vector_data_addr(*it);
              std::string size_addr = gen->get_vector_size_addr(*it);

              // Print '['
              gen->m_output << "    mov rax, 1\n";
              gen->m_output << "    mov rdi, 1\n";
              gen->m_output << "    mov rsi, __str_lbracket\n";
              gen->m_output << "    mov rdx, 1\n";
              gen->m_output << "    syscall\n";

              gen->m_output << "    mov r15, 0\n";
              gen->m_output << ".L_print_vec_loop_" << id << ":\n";
              gen->m_output << "    cmp r15, " << size_addr << "\n";
              gen->m_output << "    jge .L_print_vec_end_" << id << "\n";
              gen->m_output << "    cmp r15, 0\n";
              gen->m_output << "    je .L_print_vec_first_" << id << "\n";
              gen->m_output << "    mov rax, 1\n";
              gen->m_output << "    mov rdi, 1\n";
              gen->m_output << "    mov rsi, __str_comma_space\n";
              gen->m_output << "    mov rdx, 2\n";
              gen->m_output << "    syscall\n";
              gen->m_output << ".L_print_vec_first_" << id << ":\n";
              gen->m_output << "    mov rbx, " << data_addr << "\n";
              if (it->type == VarType::VectorInt) {
                gen->m_output << "    mov rax, qword [rbx + r15 * 8]\n";
                gen->emit_print_int_from_rax();
              } else if (it->type == VarType::VectorChar) {
                gen->m_output << "    mov rax, 1\n";
                gen->m_output << "    mov rdi, 1\n";
                gen->m_output << "    mov rsi, __str_squote\n";
                gen->m_output << "    mov rdx, 1\n";
                gen->m_output << "    syscall\n";
                gen->m_output << "    mov rbx, " << data_addr << "\n";
                gen->m_output << "    lea rsi, [rbx + r15]\n";
                gen->m_output << "    mov rax, 1\n";
                gen->m_output << "    mov rdi, 1\n";
                gen->m_output << "    mov rdx, 1\n";
                gen->m_output << "    syscall\n";
                gen->m_output << "    mov rax, 1\n";
                gen->m_output << "    mov rdi, 1\n";
                gen->m_output << "    mov rsi, __str_squote\n";
                gen->m_output << "    mov rdx, 1\n";
                gen->m_output << "    syscall\n";
              } else {
                gen->m_output << "    mov rax, 1\n";
                gen->m_output << "    mov rdi, 1\n";
                gen->m_output << "    mov rsi, __str_dquote\n";
                gen->m_output << "    mov rdx, 1\n";
                gen->m_output << "    syscall\n";
                gen->m_output << "    mov rbx, " << data_addr << "\n";
                gen->m_output << "    mov rsi, qword [rbx + r15 * 8]\n";
                gen->emit_print_string_from_rsi();
                gen->m_output << "    mov rax, 1\n";
                gen->m_output << "    mov rdi, 1\n";
                gen->m_output << "    mov rsi, __str_dquote\n";
                gen->m_output << "    mov rdx, 1\n";
                gen->m_output << "    syscall\n";
              }
              gen->m_output << "    inc r15\n";
              gen->m_output << "    jmp .L_print_vec_loop_" << id << "\n";
              gen->m_output << ".L_print_vec_end_" << id << ":\n";
              gen->m_output << "    mov rax, 1\n";
              gen->m_output << "    mov rdi, 1\n";
              gen->m_output << "    mov rsi, __str_rbracket\n";
              gen->m_output << "    mov rdx, 1\n";
              gen->m_output << "    syscall\n";
            }
          }
        }
        // Print newline at the end of print statement
        gen->m_output << "    mov rax, 1\n";
        gen->m_output << "    mov rdi, 1\n";
        gen->m_output << "    mov rsi, newline_char\n";
        gen->m_output << "    mov rdx, 1\n";
        gen->m_output << "    syscall\n";
      }

      void operator()(const NodeStmtScope *stmt_scope) const {
        gen->gen_stmt_scope(stmt_scope);
      }

      void operator()(const NodeStmtIf *stmt_if) const {
        std::string id = std::to_string(gen->m_label_count++);
        std::string end_label = ".L_endif_" + id;
        std::string next_label = stmt_if->pred.has_value() ? (".L_nextif_" + id) : end_label;

        gen->gen_expr(stmt_if->expr);
        gen->pop("rax");
        gen->m_output << "    cmp rax, 0\n";
        gen->m_output << "    je " << next_label << "\n";

        gen->gen_stmt_scope(stmt_if->scope);

        if (stmt_if->pred.has_value()) {
          gen->m_output << "    jmp " << end_label << "\n";
          gen->m_output << next_label << ":\n";
          gen->gen_if_pred(stmt_if->pred.value(), end_label);
        }

        gen->m_output << end_label << ":\n";
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
        if (is_vector_type(it->type)) {
          std::cerr << "Cannot reassign entire vector directly: "
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

      void operator()(const NodeStmtWhile *stmt_while) const {
        std::string id = std::to_string(gen->m_label_count++);
        std::string start_label = ".L_while_start_" + id;
        std::string end_label = ".L_while_end_" + id;

        gen->m_output << start_label << ":\n";
        gen->m_loops.push_back(LoopContext{
            .start_label = start_label,
            .end_label = end_label,
            .stack_size = gen->m_stack_size,
        });

        gen->gen_expr(stmt_while->expr);
        gen->pop("rax");
        gen->m_output << "    cmp rax, 0\n";
        gen->m_output << "    je " << end_label << "\n";

        gen->gen_stmt_scope(stmt_while->scope);

        gen->m_output << "    jmp " << start_label << "\n";
        gen->m_output << end_label << ":\n";

        gen->m_loops.pop_back();
      }

      void operator()(const NodeStmtBreak *) const {
        if (gen->m_loops.empty()) {
          std::cerr << "Break statement outside of while loop" << std::endl;
          exit(EXIT_FAILURE);
        }
        const LoopContext &loop = gen->m_loops.back();
        size_t slots_to_pop = gen->m_stack_size - loop.stack_size;
        if (slots_to_pop > 0) {
          gen->m_output << "    add rsp, " << (slots_to_pop * 8) << "\n";
        }
        gen->m_output << "    jmp " << loop.end_label << "\n";
      }

      void operator()(const NodeStmtContinue *) const {
        if (gen->m_loops.empty()) {
          std::cerr << "Continue statement outside of while loop" << std::endl;
          exit(EXIT_FAILURE);
        }
        const LoopContext &loop = gen->m_loops.back();
        size_t slots_to_pop = gen->m_stack_size - loop.stack_size;
        if (slots_to_pop > 0) {
          gen->m_output << "    add rsp, " << (slots_to_pop * 8) << "\n";
        }
        gen->m_output << "    jmp " << loop.start_label << "\n";
      }
    };

    StmtVisitor visitor{.gen = this};
    std::visit(visitor, stmt->var);
  }

  void gen_stmt_scope(const NodeStmtScope *stmt_scope) {
    begin_scope();
    for (const NodeStmt *stmt : stmt_scope->stmts) {
      gen_stmt(stmt);
    }
    end_scope();
  }

  void gen_if_pred(const NodeIfPred *pred, const std::string &end_label) {
    struct PredVisitor {
      Generator *gen;
      const std::string &end_label;
      void operator()(const NodeIfPredElif *elif) const {
        std::string id = std::to_string(gen->m_label_count++);
        std::string next_label = elif->pred.has_value() ? (".L_nextif_" + id) : end_label;

        gen->gen_expr(elif->expr);
        gen->pop("rax");
        gen->m_output << "    cmp rax, 0\n";
        gen->m_output << "    je " << next_label << "\n";

        gen->gen_stmt_scope(elif->scope);

        if (elif->pred.has_value()) {
          gen->m_output << "    jmp " << end_label << "\n";
          gen->m_output << next_label << ":\n";
          gen->gen_if_pred(elif->pred.value(), end_label);
        }
      }
      void operator()(const NodeIfPredElse *else_pred) const {
        gen->gen_stmt_scope(else_pred->scope);
      }
    };
    PredVisitor visitor{.gen = this, .end_label = end_label};
    std::visit(visitor, pred->var);
  }

  [[nodiscard]] std::string gen_prog() {
    m_output << "default rel\n";
    m_output << "section .text\n";
    m_output << "global _start\n";
    m_output << "_start:\n";
    m_output << "    ; Initialize heap break\n";
    m_output << "    mov rax, 12\n";
    m_output << "    xor rdi, rdi\n";
    m_output << "    syscall\n";
    m_output << "    mov [rel __heap_current], rax\n";

    for (const NodeStmt *stmt : m_prog.stmts) {
      gen_stmt(stmt);
    }

    m_output << "    mov rax, 60\n";
    m_output << "    mov rdi, 0\n";
    m_output << "    syscall\n";

    // Allocator routine
    m_output << "\n__pavaka_alloc:\n";
    m_output << "    push rbx\n";
    m_output << "    mov rbx, [rel __heap_current]\n";
    m_output << "    lea rax, [rbx + rdi]\n";
    m_output << "    mov rdi, rax\n";
    m_output << "    mov rax, 12\n";
    m_output << "    syscall\n";
    m_output << "    mov [rel __heap_current], rax\n";
    m_output << "    mov rax, rbx\n";
    m_output << "    pop rbx\n";
    m_output << "    ret\n";

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
    m_scopes.push_back(Scope{ .var_idx = m_vars.size(), .stack_size = m_stack_size });
  }

  void end_scope() {
    size_t pop_slots = m_stack_size - m_scopes.back().stack_size;
    if (pop_slots > 0) {
      m_output << "    add rsp, " << (pop_slots * 8) << "\n";
    }
    m_stack_size = m_scopes.back().stack_size;
    size_t target_vars = m_scopes.back().var_idx;
    while (m_vars.size() > target_vars) {
      m_vars.pop_back();
    }
    m_scopes.pop_back();
  }



  const NodeProg m_prog;
  std::stringstream m_output;
  size_t m_stack_size = 0;
  std::vector<Var> m_vars{};
  std::vector<Scope> m_scopes{};
  mutable std::vector<LoopContext> m_loops{};
  std::stringstream m_data;
  size_t m_string_count = 0;
  size_t m_label_count = 0;
};