#pragma once

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class TokenType {
  exit,
  int_lit,
  semi,
  open_paren,
  close_paren,
  ident,
  let,
  eq,
  plus,
  minus,
  print,
  string_lit,
  char_lit,
  star,
  fslash,
  open_curly,
  close_curly,
  string_kw,
  char_kw,
  int_kw
};

std::optional<int> bin_prec(TokenType type) {
  switch (type) {
  case TokenType::plus:
  case TokenType::minus:
    return 1;
  case TokenType::star:
  case TokenType::fslash:
    return 2;
  default:
    return {};
  }
}

struct Token {
  TokenType type;
  std::optional<std::string> value{};
};

class Tokenizer {
public:
  inline explicit Tokenizer(std::string src) : m_src(std::move(src)) {}

  inline std::vector<Token> tokenize() {
    std::vector<Token> tokens;
    std::string buf;
    while (peek().has_value()) {
      if (std::isalpha(static_cast<unsigned char>(peek().value())) ||
          peek().value() == '_') {
        buf.push_back(consume());
        while (peek().has_value() &&
               (std::isalnum(static_cast<unsigned char>(peek().value())) ||
                peek().value() == '_')) {
          buf.push_back(consume());
        }
        if (buf == "exit") {
          tokens.push_back({.type = TokenType::exit});
          buf.clear();
        } else if (buf == "print") {
          tokens.push_back({.type = TokenType::print});
          buf.clear();
        } else if (buf == "let") {
          tokens.push_back({.type = TokenType::let});
          buf.clear();
        } else if (buf == "string") {
          tokens.push_back({.type = TokenType::string_kw});
          buf.clear();
        } else if (buf == "char") {
          tokens.push_back({.type = TokenType::char_kw});
          buf.clear();
        } else if (buf == "int") {
          tokens.push_back({.type = TokenType::int_kw});
          buf.clear();
        } else {
          tokens.push_back({.type = TokenType::ident, .value = buf});
          buf.clear();
        }
      } else if (std::isdigit(static_cast<unsigned char>(peek().value()))) {
        buf.push_back(consume());
        while (peek().has_value() &&
               std::isdigit(static_cast<unsigned char>(peek().value()))) {
          buf.push_back(consume());
        }
        tokens.push_back({.type = TokenType::int_lit, .value = buf});
        buf.clear();
      } else if (peek().value() == '(') {
        consume();
        tokens.push_back({.type = TokenType::open_paren});
      } else if (peek().value() == ')') {
        consume();
        tokens.push_back({.type = TokenType::close_paren});
      } else if (peek().value() == ';') {
        consume();
        tokens.push_back({.type = TokenType::semi});
      } else if (peek().value() == '=') {
        consume();
        tokens.push_back({.type = TokenType::eq});
      } else if (peek().value() == '+') {
        consume();
        tokens.push_back({.type = TokenType::plus});
      } else if (peek().value() == '-') {
        consume();
        tokens.push_back({.type = TokenType::minus});
      } else if (peek().value() == '\'') {
        consume();

        std::string char_value;

        if (peek().has_value() && peek().value() == '\\') {
          consume();
          if (!peek().has_value()) {
            std::cerr << "Unterminated escape sequence in character literal"
                      << std::endl;
            exit(EXIT_FAILURE);
          }
          char esc = consume();
          switch (esc) {
          case 'n':
            char_value.push_back('\n');
            break;
          case 't':
            char_value.push_back('\t');
            break;
          case 'r':
            char_value.push_back('\r');
            break;
          case '0':
            char_value.push_back('\0');
            break;
          case '\'':
            char_value.push_back('\'');
            break;
          case '\\':
            char_value.push_back('\\');
            break;
          default:
            char_value.push_back(esc);
            break;
          }
        } else if (peek().has_value() && peek().value() != '\'') {
          char_value.push_back(consume());
        } else {
          std::cerr << "Empty character literal" << std::endl;
          exit(EXIT_FAILURE);
        }

        if (!peek().has_value() || peek().value() != '\'') {
          std::cerr << "Unterminated character literal" << std::endl;
          exit(EXIT_FAILURE);
        }
        consume();

        tokens.push_back({.type = TokenType::char_lit, .value = char_value});
      } else if (peek().value() == '"') {
        consume();

        std::string string_value;

        while (peek().has_value() && peek().value() != '"') {
          if (peek().value() == '\\') {
            consume();
            if (!peek().has_value()) {
              std::cerr << "Unterminated escape sequence in string literal"
                        << std::endl;
              exit(EXIT_FAILURE);
            }
            char esc = consume();
            switch (esc) {
            case 'n':
              string_value.push_back('\n');
              break;
            case 't':
              string_value.push_back('\t');
              break;
            case 'r':
              string_value.push_back('\r');
              break;
            case '0':
              string_value.push_back('\0');
              break;
            case '"':
              string_value.push_back('"');
              break;
            case '\\':
              string_value.push_back('\\');
              break;
            default:
              string_value.push_back(esc);
              break;
            }
          } else {
            string_value.push_back(consume());
          }
        }

        if (!peek().has_value()) {
          std::cerr << "Unterminated string literal" << std::endl;
          exit(EXIT_FAILURE);
        }
        consume();

        tokens.push_back(
            {.type = TokenType::string_lit, .value = string_value});
      } else if (peek().value() == '*') {
        consume();
        tokens.push_back({.type = TokenType::star});
      } else if (peek().value() == '/') {
        consume();
        tokens.push_back({.type = TokenType::fslash});
      } else if (peek().value() == '{') {
        consume();
        tokens.push_back({.type = TokenType::open_curly});
      } else if (peek().value() == '}') {
        consume();
        tokens.push_back({.type = TokenType::close_curly});
      } else if (std::isspace(static_cast<unsigned char>(peek().value()))) {
        consume();
        continue;
      } else {
        std::cerr << "You messed up!" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    m_index = 0;
    return tokens;
  }

private:
  [[nodiscard]] inline std::optional<char> peek(int offset = 0) const {
    if (m_index + offset >= m_src.length()) {
      return {};
    } else {
      return m_src.at(m_index + offset);
    }
  }

  inline char consume() { return m_src.at(m_index++); }

  const std::string m_src;
  size_t m_index = 0;
};