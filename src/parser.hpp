#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "./arena.hpp"
#include "tokenization.hpp"

enum class VarType {
  Int,
  String,
  Char,
  VectorInt,
  VectorChar,
  VectorString
};

inline bool is_vector_type(VarType t) {
  return t == VarType::VectorInt || t == VarType::VectorChar || t == VarType::VectorString;
}

inline VarType vector_elem_type(VarType t) {
  switch (t) {
  case VarType::VectorInt:
    return VarType::Int;
  case VarType::VectorChar:
    return VarType::Char;
  case VarType::VectorString:
    return VarType::String;
  default:
    return VarType::Int;
  }
}

struct NodeExpr;

struct NodeStmtPrint {
  std::vector<NodeExpr *> exprs;
};

struct NodeTermIntLit {
  Token int_lit;
};

struct NodeTermCharLit {
  Token char_lit;
};

struct NodeTermStringLit {
  Token string_lit;
};

struct NodeTermIdent {
  Token ident;
};

struct NodeTermParen {
  NodeExpr *expr;
};

struct NodeTermVectorAccess {
  Token ident;
  NodeExpr *index;
};

struct NodeTermVectorSize {
  Token ident;
};

struct NodeBinExprAdd {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprMulti {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprSub {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprDiv {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprEq {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprNotEq {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprLess {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprLessEq {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprGreater {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExprGreaterEq {
  NodeExpr *lhs;
  NodeExpr *rhs;
};

struct NodeBinExpr {
  std::variant<NodeBinExprAdd *, NodeBinExprMulti *, NodeBinExprSub *,
               NodeBinExprDiv *, NodeBinExprEq *, NodeBinExprNotEq *,
               NodeBinExprLess *, NodeBinExprLessEq *, NodeBinExprGreater *,
               NodeBinExprGreaterEq *>
      var;
};

struct NodeTerm {
  std::variant<NodeTermIntLit *, NodeTermIdent *, NodeTermParen *,
               NodeTermCharLit *, NodeTermStringLit *,
               NodeTermVectorAccess *, NodeTermVectorSize *>
      var;
};

struct NodeExpr {
  std::variant<NodeTerm *, NodeBinExpr *> var;
};

struct NodeStmtExit {
  NodeExpr *expr;
};

struct NodeStmtLet {
  VarType type;
  Token ident;
  NodeExpr *expr;
};

struct NodeStmt;

struct NodeStmtScope {
  std::vector<NodeStmt*>stmts;
};

struct NodeStmtAssign {
  Token ident;
  NodeExpr *expr;
};

struct NodeIfPred;

struct NodeStmtIf {
  NodeExpr *expr;
  NodeStmtScope *scope;
  std::optional<NodeIfPred *> pred;
};

struct NodeIfPredElif {
  NodeExpr *expr;
  NodeStmtScope *scope;
  std::optional<NodeIfPred *> pred;
};

struct NodeIfPredElse {
  NodeStmtScope *scope;
};

struct NodeIfPred {
  std::variant<NodeIfPredElif *, NodeIfPredElse *> var;
};

struct NodeStmtLetVector {
  VarType type;
  Token ident;
  std::vector<NodeExpr *> init_elements;
};

struct NodeStmtVectorPush {
  Token ident;
  NodeExpr *expr;
};

struct NodeStmtVectorPop {
  Token ident;
};

struct NodeStmtVectorAssign {
  Token ident;
  NodeExpr *index;
  NodeExpr *value;
};

struct NodeStmt {
  std::variant<NodeStmtExit *, NodeStmtLet *, NodeStmtPrint *, NodeStmtScope *,
               NodeStmtAssign *, NodeStmtIf *,
               NodeStmtLetVector *, NodeStmtVectorPush *, NodeStmtVectorPop *,
               NodeStmtVectorAssign *>
      var;
};

struct NodeProg {
  std::vector<NodeStmt *> stmts;
};

class Parser {
public:
  inline explicit Parser(std::vector<Token> tokens)
      : m_tokens(std::move(tokens)), m_allocator(1024 * 1024 * 4) // 4 mb
  {}

  std::optional<NodeTerm *> parse_term() {
    if (auto int_lit = try_consume(TokenType::int_lit)) {
      auto term_int_lit = m_allocator.alloc<NodeTermIntLit>();
      term_int_lit->int_lit = int_lit.value();
      auto term = m_allocator.alloc<NodeTerm>();
      term->var = term_int_lit;
      return term;
    } else if (auto char_lit = try_consume(TokenType::char_lit)) {
      auto term_char_lit = m_allocator.alloc<NodeTermCharLit>();
      term_char_lit->char_lit = char_lit.value();
      auto term = m_allocator.alloc<NodeTerm>();
      term->var = term_char_lit;
      return term;
    } else if (auto string_lit = try_consume(TokenType::string_lit)) {
      auto term_string_lit = m_allocator.alloc<NodeTermStringLit>();
      term_string_lit->string_lit = string_lit.value();
      auto term = m_allocator.alloc<NodeTerm>();
      term->var = term_string_lit;
      return term;
    } else if (peek().has_value() && peek().value().type == TokenType::ident) {
      Token ident = consume();
      if (peek().has_value() && peek().value().type == TokenType::open_bracket) {
        consume(); // consume '['
        auto index_expr = parse_expr();
        if (!index_expr.has_value()) {
          std::cerr << "Expected expression inside `[]`" << std::endl;
          exit(EXIT_FAILURE);
        }
        try_consume(TokenType::close_bracket, "Expected `]` after vector index");
        auto term_acc = m_allocator.alloc<NodeTermVectorAccess>();
        term_acc->ident = ident;
        term_acc->index = index_expr.value();
        auto term = m_allocator.alloc<NodeTerm>();
        term->var = term_acc;
        return term;
      } else if (peek().has_value() && peek().value().type == TokenType::dot) {
        consume(); // consume '.'
        Token method = try_consume(TokenType::ident, "Expected method after `.`");
        if (method.value.value() == "size") {
          try_consume(TokenType::open_paren, "Expected `(` after `size`");
          try_consume(TokenType::close_paren, "Expected `)` after `(` in size()");
          auto term_size = m_allocator.alloc<NodeTermVectorSize>();
          term_size->ident = ident;
          auto term = m_allocator.alloc<NodeTerm>();
          term->var = term_size;
          return term;
        } else {
          std::cerr << "Unknown vector method in expression: " << method.value.value() << std::endl;
          exit(EXIT_FAILURE);
        }
      } else {
        auto expr_ident = m_allocator.alloc<NodeTermIdent>();
        expr_ident->ident = ident;
        auto term = m_allocator.alloc<NodeTerm>();
        term->var = expr_ident;
        return term;
      }
    } else if (peek().has_value() && peek().value().type == TokenType::dollar) {
      consume();
      try_consume(TokenType::open_paren, "Expected `(` after `$`");
      auto expr = parse_expr();
      if (!expr.has_value()) {
        std::cerr << "Expected expression inside `$()`" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::close_paren, "Expected `)` after expression inside `$()`");
      auto term_paren = m_allocator.alloc<NodeTermParen>();
      term_paren->expr = expr.value();
      auto term = m_allocator.alloc<NodeTerm>();
      term->var = term_paren;
      return term;
    } else if (auto open_paren = try_consume(TokenType::open_paren)) {
      auto expr = parse_expr();
      if (!expr.has_value()) {
        std::cerr << "Expected expression" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::close_paren, "Expected `)`");
      auto term_paren = m_allocator.alloc<NodeTermParen>();
      term_paren->expr = expr.value();
      auto term = m_allocator.alloc<NodeTerm>();
      term->var = term_paren;
      return term;
    } else {
      return {};
    }
  }

  std::optional<NodeExpr *> parse_expr(int min_prec = 0) {
    std::optional<NodeTerm *> term_lhs = parse_term();
    if (!term_lhs.has_value()) {
      return {};
    }

    auto expr_lhs = m_allocator.alloc<NodeExpr>();
    expr_lhs->var = term_lhs.value();

    while (true) {
      std::optional<Token> curr_tok = peek();
      std::optional<int> prec;
      if (curr_tok.has_value()) {
        prec = bin_prec(curr_tok->type);
        if (!prec.has_value() || prec < min_prec) {
          break;
        }
      } else {
        break;
      }

      Token op = consume();

      int next_min_prec = prec.value() + 1;
      auto expr_rhs = parse_expr(next_min_prec);
      if (!expr_rhs.has_value()) {
        std::cerr << "Unable to parse expression" << std::endl;
        exit(EXIT_FAILURE);
      }

      auto expr = m_allocator.alloc<NodeBinExpr>();
      auto expr_lhs2 = m_allocator.alloc<NodeExpr>();
      expr_lhs2->var = expr_lhs->var;

      if (op.type == TokenType::plus) {
        auto add = m_allocator.alloc<NodeBinExprAdd>();

        add->lhs = expr_lhs2;
        add->rhs = expr_rhs.value();
        expr->var = add;
      } else if (op.type == TokenType::star) {
        auto multi = m_allocator.alloc<NodeBinExprMulti>();
        multi->lhs = expr_lhs2;
        multi->rhs = expr_rhs.value();
        expr->var = multi;
      } else if (op.type == TokenType::minus) {
        auto sub = m_allocator.alloc<NodeBinExprSub>();
        sub->lhs = expr_lhs2;
        sub->rhs = expr_rhs.value();
        expr->var = sub;
      } else if (op.type == TokenType::fslash) {
        auto div = m_allocator.alloc<NodeBinExprDiv>();
        div->lhs = expr_lhs2;
        div->rhs = expr_rhs.value();
        expr->var = div;
      } else if (op.type == TokenType::double_eq) {
        auto eq = m_allocator.alloc<NodeBinExprEq>();
        eq->lhs = expr_lhs2;
        eq->rhs = expr_rhs.value();
        expr->var = eq;
      } else if (op.type == TokenType::not_equal) {
        auto neq = m_allocator.alloc<NodeBinExprNotEq>();
        neq->lhs = expr_lhs2;
        neq->rhs = expr_rhs.value();
        expr->var = neq;
      } else if (op.type == TokenType::less) {
        auto less = m_allocator.alloc<NodeBinExprLess>();
        less->lhs = expr_lhs2;
        less->rhs = expr_rhs.value();
        expr->var = less;
      } else if (op.type == TokenType::less_eq) {
        auto less_eq = m_allocator.alloc<NodeBinExprLessEq>();
        less_eq->lhs = expr_lhs2;
        less_eq->rhs = expr_rhs.value();
        expr->var = less_eq;
      } else if (op.type == TokenType::greater) {
        auto greater = m_allocator.alloc<NodeBinExprGreater>();
        greater->lhs = expr_lhs2;
        greater->rhs = expr_rhs.value();
        expr->var = greater;
      } else if (op.type == TokenType::greater_eq) {
        auto greater_eq = m_allocator.alloc<NodeBinExprGreaterEq>();
        greater_eq->lhs = expr_lhs2;
        greater_eq->rhs = expr_rhs.value();
        expr->var = greater_eq;
      }

      expr_lhs->var = expr;
    }
    return expr_lhs;
  }

  std::optional<NodeStmt *> parse_stmt() {
    if (peek().has_value() && peek().value().type == TokenType::print &&
        peek(1).has_value() && peek(1).value().type == TokenType::open_paren) {
      consume();
      consume();

      std::vector<NodeExpr *> exprs;
      while (peek().has_value() && peek().value().type != TokenType::close_paren) {
        auto expr = parse_expr();
        if (!expr.has_value()) {
          std::cerr << "Expected expression in print statement" << std::endl;
          exit(EXIT_FAILURE);
        }
        exprs.push_back(expr.value());

        if (peek().has_value() && peek().value().type == TokenType::comma) {
          consume();
        }
      }

      try_consume(TokenType::close_paren, "Expected `)`");
      try_consume(TokenType::semi, "Expected `;`");

      auto stmt_print = m_allocator.alloc<NodeStmtPrint>();
      stmt_print->exprs = exprs;

      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_print;

      return stmt;
    } else if (peek().has_value() && peek().value().type == TokenType::exit &&
               peek(1).has_value() &&
               peek(1).value().type == TokenType::open_paren) {
      consume();
      consume();
      auto stmt_exit = m_allocator.alloc<NodeStmtExit>();
      if (auto node_expr = parse_expr()) {
        stmt_exit->expr = node_expr.value();
      } else {
        std::cerr << "Invalid expression" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::close_paren, "Expected `)`");
      try_consume(TokenType::semi, "Expected `;`");
      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_exit;
      return stmt;
    } else if (peek().has_value() && peek().value().type == TokenType::let) {
      consume();
      if (peek().has_value() && peek().value().type == TokenType::vector_kw) {
        consume(); // consume 'vector'
        try_consume(TokenType::less, "Expected `<` after `vector`");
        VarType elem_type;
        if (auto int_kw = try_consume(TokenType::int_kw)) {
          elem_type = VarType::Int;
        } else if (auto char_kw = try_consume(TokenType::char_kw)) {
          elem_type = VarType::Char;
        } else if (auto string_kw = try_consume(TokenType::string_kw)) {
          elem_type = VarType::String;
        } else {
          std::cerr << "Expected type (int, char, string) inside `vector<...>`" << std::endl;
          exit(EXIT_FAILURE);
        }
        try_consume(TokenType::greater, "Expected `>` after vector type");
        Token ident = try_consume(TokenType::ident, "Expected identifier for vector");

        VarType vec_type;
        if (elem_type == VarType::Int) vec_type = VarType::VectorInt;
        else if (elem_type == VarType::Char) vec_type = VarType::VectorChar;
        else vec_type = VarType::VectorString;

        auto stmt_vec = m_allocator.alloc<NodeStmtLetVector>();
        stmt_vec->type = vec_type;
        stmt_vec->ident = ident;

        if (peek().has_value() && peek().value().type == TokenType::eq) {
          consume(); // consume '='
          try_consume(TokenType::open_curly, "Expected `{` for vector initializer list");
          while (peek().has_value() && peek().value().type != TokenType::close_curly) {
            auto elem_expr = parse_expr();
            if (!elem_expr.has_value()) {
              std::cerr << "Expected expression in vector initializer list" << std::endl;
              exit(EXIT_FAILURE);
            }
            stmt_vec->init_elements.push_back(elem_expr.value());
            if (peek().has_value() && peek().value().type == TokenType::comma) {
              consume();
            }
          }
          try_consume(TokenType::close_curly, "Expected `}` after vector initializer list");
        }
        try_consume(TokenType::semi, "Expected `;` after vector declaration");
        auto stmt = m_allocator.alloc<NodeStmt>();
        stmt->var = stmt_vec;
        return stmt;
      }

      VarType var_type = VarType::Int;
      if (peek().has_value() && peek().value().type == TokenType::string_kw) {
        consume();
        var_type = VarType::String;
      } else if (peek().has_value() &&
                 peek().value().type == TokenType::char_kw) {
        consume();
        var_type = VarType::Char;
      } else if (peek().has_value() &&
                 peek().value().type == TokenType::int_kw) {
        consume();
        var_type = VarType::Int;
      }

      Token ident = try_consume(TokenType::ident, "Expected identifier");
      try_consume(TokenType::eq, "Expected `=`");

      auto stmt_let = m_allocator.alloc<NodeStmtLet>();
      stmt_let->type = var_type;
      stmt_let->ident = ident;
      if (auto expr = parse_expr()) {
        stmt_let->expr = expr.value();
      } else {
        std::cerr << "Invalid expression" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::semi, "Expected `;`");
      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_let;
      return stmt;
    } else if (peek().has_value() &&
               peek().value().type == TokenType::ident &&
               peek(1).has_value() &&
               peek(1).value().type == TokenType::dot) {
      Token ident = consume();
      consume(); // consume '.'
      Token method = try_consume(TokenType::ident, "Expected method after `.`");
      if (method.value.value() == "push_back") {
        try_consume(TokenType::open_paren, "Expected `(` after `push_back`");
        auto expr = parse_expr();
        if (!expr.has_value()) {
          std::cerr << "Expected expression inside `push_back(...)`" << std::endl;
          exit(EXIT_FAILURE);
        }
        try_consume(TokenType::close_paren, "Expected `)` after expression in `push_back`");
        try_consume(TokenType::semi, "Expected `;` after `push_back(...)`");
        auto stmt_push = m_allocator.alloc<NodeStmtVectorPush>();
        stmt_push->ident = ident;
        stmt_push->expr = expr.value();
        auto stmt = m_allocator.alloc<NodeStmt>();
        stmt->var = stmt_push;
        return stmt;
      } else if (method.value.value() == "pop_back") {
        try_consume(TokenType::open_paren, "Expected `(` after `pop_back`");
        try_consume(TokenType::close_paren, "Expected `)` after `(` in `pop_back`");
        try_consume(TokenType::semi, "Expected `;` after `pop_back()`");
        auto stmt_pop = m_allocator.alloc<NodeStmtVectorPop>();
        stmt_pop->ident = ident;
        auto stmt = m_allocator.alloc<NodeStmt>();
        stmt->var = stmt_pop;
        return stmt;
      } else {
        std::cerr << "Unknown method call on identifier: " << method.value.value() << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (peek().has_value() &&
               peek().value().type == TokenType::ident &&
               peek(1).has_value() &&
               peek(1).value().type == TokenType::open_bracket) {
      Token ident = consume();
      consume(); // consume '['
      auto index_expr = parse_expr();
      if (!index_expr.has_value()) {
        std::cerr << "Expected index expression inside `[]`" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::close_bracket, "Expected `]` after index");
      try_consume(TokenType::eq, "Expected `=` for index assignment");
      auto val_expr = parse_expr();
      if (!val_expr.has_value()) {
        std::cerr << "Expected expression on right side of assignment" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::semi, "Expected `;` after index assignment");
      auto stmt_assign = m_allocator.alloc<NodeStmtVectorAssign>();
      stmt_assign->ident = ident;
      stmt_assign->index = index_expr.value();
      stmt_assign->value = val_expr.value();
      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_assign;
      return stmt;
    } else if (peek().has_value() &&
               peek().value().type == TokenType::ident &&
               peek(1).has_value() &&
               peek(1).value().type == TokenType::eq) {
      auto stmt_assign = m_allocator.alloc<NodeStmtAssign>();
      stmt_assign->ident = consume();
      consume();
      if (auto expr = parse_expr()) {
        stmt_assign->expr = expr.value();
      } else {
        std::cerr << "Invalid expression" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::semi, "Expected `;`");
      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_assign;
      return stmt;
    } else if (peek().has_value() &&
               peek().value().type == TokenType::if_kw) {
      consume(); // consume 'if'
      try_consume(TokenType::open_paren, "Expected `(` after `if`");
      auto expr = parse_expr();
      if (!expr.has_value()) {
        std::cerr << "Expected expression in if statement" << std::endl;
        exit(EXIT_FAILURE);
      }
      try_consume(TokenType::close_paren, "Expected `)` after expression");
      auto scope = parse_scope();
      if (!scope.has_value()) {
        std::cerr << "Expected `{` in if statement" << std::endl;
        exit(EXIT_FAILURE);
      }
      auto stmt_if = m_allocator.alloc<NodeStmtIf>();
      stmt_if->expr = expr.value();
      stmt_if->scope = scope.value();
      stmt_if->pred = parse_if_pred();
      auto stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_if;
      return stmt;
    } else if (peek().has_value() &&
               peek().value().type == TokenType::open_curly) {
      if (auto stmt_scope = parse_scope()) {
        auto stmt = m_allocator.alloc<NodeStmt>();
        stmt->var = stmt_scope.value();
        return stmt;
      }
      return {};
    } else {
      return {};
    }
  }

  std::optional<NodeIfPred *> parse_if_pred() {
    if (peek().has_value() && peek().value().type == TokenType::else_kw) {
      consume(); // consume 'else'
      if (peek().has_value() && peek().value().type == TokenType::if_kw) {
        consume(); // consume 'if'
        try_consume(TokenType::open_paren, "Expected `(` after `if`");
        auto expr = parse_expr();
        if (!expr.has_value()) {
          std::cerr << "Expected expression in else if" << std::endl;
          exit(EXIT_FAILURE);
        }
        try_consume(TokenType::close_paren, "Expected `)` after expression");
        auto scope = parse_scope();
        if (!scope.has_value()) {
          std::cerr << "Expected `{` in else if" << std::endl;
          exit(EXIT_FAILURE);
        }
        auto elif = m_allocator.alloc<NodeIfPredElif>();
        elif->expr = expr.value();
        elif->scope = scope.value();
        elif->pred = parse_if_pred();
        auto pred = m_allocator.alloc<NodeIfPred>();
        pred->var = elif;
        return pred;
      } else {
        auto scope = parse_scope();
        if (!scope.has_value()) {
          std::cerr << "Expected `{` after `else`" << std::endl;
          exit(EXIT_FAILURE);
        }
        auto else_pred = m_allocator.alloc<NodeIfPredElse>();
        else_pred->scope = scope.value();
        auto pred = m_allocator.alloc<NodeIfPred>();
        pred->var = else_pred;
        return pred;
      }
    }
    return {};
  }

  std::optional<NodeStmtScope *> parse_scope() {
    if (!try_consume(TokenType::open_curly).has_value()) {
      return {};
    }
    auto stmt_scope = m_allocator.alloc<NodeStmtScope>();
    while (auto stmt = parse_stmt()) {
      stmt_scope->stmts.push_back(stmt.value());
    }
    try_consume(TokenType::close_curly, "Expected `}`");
    return stmt_scope;
  }

  std::optional<NodeProg> parse_prog() {
    NodeProg prog;
    while (peek().has_value()) {
      if (auto stmt = parse_stmt()) {
        prog.stmts.push_back(stmt.value());
      } else {
        std::cerr << "Invalid statement" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    return prog;
  }

private:
  [[nodiscard]] inline std::optional<Token> peek(std::size_t offset = 0) const {
    if (m_index + offset >= m_tokens.size()) {
      return {};
    } else {
      return m_tokens.at(m_index + offset);
    }
  }

  inline Token consume() { return m_tokens.at(m_index++); }

  inline Token try_consume(TokenType type, const std::string &err_msg) {
    if (peek().has_value() && peek().value().type == type) {
      return consume();
    } else {
      std::cerr << err_msg << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  inline std::optional<Token> try_consume(TokenType type) {
    if (peek().has_value() && peek().value().type == type) {
      return consume();
    } else {
      return {};
    }
  }

  const std::vector<Token> m_tokens;
  size_t m_index = 0;
  ArenaAllocator m_allocator;
};