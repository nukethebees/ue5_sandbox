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

            auto const value{peek()};
            if (is_identifier_start(value)) {
                result.push_back(lex_identifier());
            } else if (std::isdigit(static_cast<unsigned char>(value)) != 0 ||
                       (value == '-' &&
                        std::isdigit(static_cast<unsigned char>(peek_next())) != 0)) {
                result.push_back(lex_number());
            } else if (value == '"') {
                result.push_back(lex_string());
            } else {
                result.push_back(lex_punctuation());
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

    static auto is_identifier_start(char const value) -> bool {
        return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_';
    }

    static auto is_identifier_continue(char const value) -> bool {
        return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
    }

    void skip_trivia() {
        while (!at_end()) {
            if (std::isspace(static_cast<unsigned char>(peek())) != 0) {
                advance();
                continue;
            }
            if (peek() == '/' && peek_next() == '/') {
                while (!at_end() && peek() != '\n') {
                    advance();
                }
                continue;
            }
            return;
        }
    }

    auto lex_identifier() -> Token {
        auto const span{current_span()};
        std::string text;
        while (!at_end() && is_identifier_continue(peek())) {
            text.push_back(advance());
        }
        return Token{TokenKind::identifier, std::move(text), span};
    }

    auto lex_number() -> Token {
        auto const span{current_span()};
        std::string text;
        if (peek() == '-') {
            text.push_back(advance());
        }
        while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            text.push_back(advance());
        }
        if (peek() == '.') {
            text.push_back(advance());
            if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
                throw SourceError{path_, current_span(), "expected a digit after decimal point"};
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                text.push_back(advance());
            }
        }
        return Token{TokenKind::number, std::move(text), span};
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

    auto lex_punctuation() -> Token {
        auto const span{current_span()};
        auto const value{advance()};
        switch (value) {
        case '{':
            return Token{TokenKind::left_brace, "{", span};
        case '}':
            return Token{TokenKind::right_brace, "}", span};
        case '(':
            return Token{TokenKind::left_parenthesis, "(", span};
        case ')':
            return Token{TokenKind::right_parenthesis, ")", span};
        case ',':
            return Token{TokenKind::comma, ",", span};
        case '=':
            return Token{TokenKind::equal, "=", span};
        case '<':
            return Token{TokenKind::less, "<", span};
        case '>':
            return Token{TokenKind::greater, ">", span};
        case ':':
            if (peek() == ':') {
                advance();
                return Token{TokenKind::scope, "::", span};
            }
            break;
        default:
            break;
        }
        throw SourceError{path_, span, "unexpected character '" + std::string{value} + "'"};
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
