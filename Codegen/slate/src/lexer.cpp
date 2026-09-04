#include "lexer.h"

#include <cctype>
#include <string>
#include <utility>

namespace slate_codegen::detail {
namespace {

class Lexer {
  public:
    Lexer(std::string_view const path, std::string_view const source)
        : path_{path}, source_{source} {}

    auto lex() -> std::vector<Token> {
        std::vector<Token> result;
        while (true) {
            skip_trivia();
            auto const span{current_span()};
            if (at_end()) {
                result.push_back(Token{TokenKind::end, {}, span});
                return result;
            }

            if (peek() == '(') {
                advance();
                result.push_back(Token{TokenKind::left_parenthesis, "(", span});
            } else if (peek() == ')') {
                advance();
                result.push_back(Token{TokenKind::right_parenthesis, ")", span});
            } else if (peek() == '"') {
                result.push_back(lex_string());
            } else if (peek() == ':' && peek_next() != ':') {
                result.push_back(lex_keyword());
            } else {
                result.push_back(lex_atom());
            }
        }
    }

  private:
    auto current_span() const -> SourceSpan { return SourceSpan{line_, column_}; }

    auto at_end() const -> bool { return index_ >= source_.size(); }

    auto peek() const -> char { return at_end() ? '\0' : source_[index_]; }

    auto peek_next() const -> char {
        return index_ + 1 >= source_.size() ? '\0' : source_[index_ + 1];
    }

    auto advance() -> char {
        auto const value{source_[index_++]};
        if (value == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return value;
    }

    static auto is_delimiter(char const value) -> bool {
        return value == '\0' || value == '(' || value == ')' || value == '"' || value == ';' ||
               std::isspace(static_cast<unsigned char>(value)) != 0;
    }

    void skip_trivia() {
        while (!at_end()) {
            if (std::isspace(static_cast<unsigned char>(peek())) != 0) {
                advance();
                continue;
            }
            if (peek() == ';') {
                while (!at_end() && peek() != '\n') {
                    advance();
                }
                continue;
            }
            return;
        }
    }

    auto lex_atom() -> Token {
        auto const span{current_span()};
        std::string text;
        while (!is_delimiter(peek())) {
            text.push_back(advance());
        }
        if (text.empty()) {
            throw SourceError{path_, span, "unexpected character '" + std::string{peek()} + "'"};
        }
        return Token{TokenKind::atom, std::move(text), span};
    }

    auto lex_keyword() -> Token {
        auto const span{current_span()};
        advance();
        auto token{lex_atom()};
        token.kind = TokenKind::keyword;
        token.span = span;
        return token;
    }

    auto lex_string() -> Token {
        auto const span{current_span()};
        advance();
        std::string text;
        while (!at_end() && peek() != '"') {
            auto value{advance()};
            if (value == '\n') {
                throw SourceError{path_, span, "unterminated text literal"};
            }
            if (value != '\\') {
                text.push_back(value);
                continue;
            }
            if (at_end()) {
                throw SourceError{path_, span, "unterminated text literal"};
            }
            value = advance();
            switch (value) {
            case '\\':
            case '"':
                text.push_back(value);
                break;
            case 'n':
                text.push_back('\n');
                break;
            case 'r':
                text.push_back('\r');
                break;
            case 't':
                text.push_back('\t');
                break;
            default:
                throw SourceError{path_, current_span(), "unsupported text escape"};
            }
        }
        if (at_end()) {
            throw SourceError{path_, span, "unterminated text literal"};
        }
        advance();
        return Token{TokenKind::string, std::move(text), span};
    }

    std::string_view path_;
    std::string_view source_;
    std::size_t index_{};
    std::size_t line_{1};
    std::size_t column_{1};
};

}

auto lex(std::string_view const path, std::string_view const source) -> std::vector<Token> {
    return Lexer{path, source}.lex();
}

}
