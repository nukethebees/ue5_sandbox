#include <codegen/sexpr/lexer.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <codegen/sexpr/token_range.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace lispb::codegen::sexpr {
namespace {

class Lexer {
  public:
    Lexer(Syntax& syntax, SourceFileId const file)
        : syntax_{syntax}
        , file_{file}
        , source_{syntax.sources().text_id(file)}
        , length_{static_cast<std::uint32_t>(syntax.sources().text(file).size())} {}

    auto lex() -> TokenRange {
        auto const first{syntax_.token_count()};
        while (true) {
            skip_trivia();
            auto const begin{index_};
            if (at_end()) {
                emit(TokenKind::end, {}, begin);
                return {first, syntax_.token_count() - first};
            }

            if (lex_raw_literal()) {
                continue;
            }
            if (peek() == '(' || peek() == ')') {
                auto const kind{peek() == '(' ? TokenKind::left_parenthesis
                                              : TokenKind::right_parenthesis};
                ++index_;
                emit(kind, slice(begin, index_), begin);
            } else if (peek() == '"') {
                lex_string();
            } else if (peek() == ':' && peek_next() != ':') {
                ++index_;
                lex_atom(TokenKind::keyword, begin);
            } else {
                lex_atom(TokenKind::atom, begin);
            }
        }
    }
  private:
    [[nodiscard]] auto source() const -> std::string_view {
        return syntax_.strings().view(source_);
    }

    [[nodiscard]] auto span(std::uint32_t const begin) const -> SourceSpan {
        return {file_, begin, index_, {}};
    }

    [[nodiscard]] auto at_end() const -> bool { return index_ == length_; }
    [[nodiscard]] auto peek() const -> char { return at_end() ? '\0' : source()[index_]; }
    [[nodiscard]] auto peek_next() const -> char {
        return index_ + 1 >= length_ ? '\0' : source()[index_ + 1];
    }

    [[nodiscard]] auto slice(std::uint32_t const begin, std::uint32_t const end)
        -> lispb::StringId {
        return syntax_.strings().slice(source_, begin, end - begin);
    }

    void emit(TokenKind const kind,
              lispb::StringId const text,
              std::uint32_t const begin,
              lispb::IdentifierId const tag = {}) {
        static_cast<void>(syntax_.add_token(kind, text, span(begin), tag));
    }

    [[noreturn]] void fail(std::uint32_t const begin, std::string const& message) const {
        throw SourceError{syntax_.sources(), span(begin), message};
    }

    static auto is_delimiter(char const value) -> bool {
        return value == '\0' || value == '(' || value == ')' || value == '"' || value == ';' ||
               std::isspace(static_cast<unsigned char>(value)) != 0;
    }

    static auto is_tag_start(char const value) -> bool { return value >= 'a' && value <= 'z'; }
    static auto is_tag_character(char const value) -> bool {
        return is_tag_start(value) || (value >= '0' && value <= '9') || value == '-';
    }

    auto lex_raw_literal() -> bool {
        if (peek() != '#' || index_ + 1 >= length_ || !is_tag_start(peek_next())) {
            return false;
        }

        auto tag_end{index_ + 2};
        while (tag_end < length_ && is_tag_character(source()[tag_end])) {
            ++tag_end;
        }
        if (tag_end >= length_ || source()[tag_end] != '{') {
            return false;
        }

        auto const begin{index_};
        auto const tag{syntax_.strings().intern(slice(begin + 1, tag_end))};
        auto const closing{"}" + std::string{syntax_.strings().view(tag)} + "#"};
        index_ = tag_end + 1;
        auto const payload_start{index_};
        auto const closing_start{source().find(closing, payload_start)};
        if (closing_start == std::string_view::npos) {
            fail(begin,
                 "unterminated raw literal '#" + std::string{syntax_.strings().view(tag)} +
                     "{'; expected '" + closing + "'");
        }

        auto const text{slice(payload_start, static_cast<std::uint32_t>(closing_start))};
        index_ = static_cast<std::uint32_t>(closing_start + closing.size());
        emit(TokenKind::raw_literal, text, begin, tag);
        return true;
    }

    void skip_trivia() {
        while (!at_end()) {
            if (std::isspace(static_cast<unsigned char>(peek())) != 0) {
                ++index_;
            } else if (peek() == ';') {
                while (!at_end() && peek() != '\n') {
                    ++index_;
                }
            } else {
                return;
            }
        }
    }

    void lex_atom(TokenKind const kind, std::uint32_t const begin) {
        auto const text_begin{index_};
        while (!is_delimiter(peek())) {
            ++index_;
        }
        if (index_ == text_begin) {
            fail(text_begin, "unexpected character '" + std::string{peek()} + "'");
        }
        emit(kind, slice(text_begin, index_), begin);
    }

    void lex_string() {
        auto const begin{index_++};
        auto const text_begin{index_};
        auto chunk_begin{index_};
        std::string decoded;
        bool escaped{};
        while (!at_end() && peek() != '"') {
            auto value{source()[index_++]};
            if (value == '\n') {
                fail(begin, "unterminated text literal");
            }
            if (value != '\\') {
                continue;
            }

            escaped = true;
            decoded.append(source().substr(chunk_begin, index_ - chunk_begin - 1));
            if (at_end()) {
                fail(begin, "unterminated text literal");
            }
            value = source()[index_++];
            switch (value) {
                case '\\':
                case '"':
                    decoded.push_back(value);
                    break;
                case 'n':
                    decoded.push_back('\n');
                    break;
                case 'r':
                    decoded.push_back('\r');
                    break;
                case 't':
                    decoded.push_back('\t');
                    break;
                default:
                    fail(index_, "unsupported text escape");
            }
            chunk_begin = index_;
        }
        if (at_end()) {
            fail(begin, "unterminated text literal");
        }

        lispb::StringId text;
        if (escaped) {
            decoded.append(source().substr(chunk_begin, index_ - chunk_begin));
            text = syntax_.strings().add(decoded);
        } else {
            text = slice(text_begin, index_);
        }
        ++index_;
        emit(TokenKind::string, text, begin);
    }

    Syntax& syntax_;
    SourceFileId file_;
    lispb::StringId source_;
    std::uint32_t length_{};
    std::uint32_t index_{};
};

} // namespace

auto lex(Syntax& syntax, SourceFileId const file) -> TokenRange {
    return Lexer{syntax, file}.lex();
}

} // namespace lispb::codegen::sexpr
