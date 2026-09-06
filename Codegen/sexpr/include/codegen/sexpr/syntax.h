#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace codegen::sexpr {

struct SourceSpan {
    std::size_t line{1};
    std::size_t column{1};
    std::string path;
    std::string expansion;
};

class SourceError final : public std::runtime_error {
  public:
    SourceError(std::string_view const path, SourceSpan const span, std::string const& message)
        : std::runtime_error{(span.path.empty() ? std::string{path} : span.path) + ":" +
                             std::to_string(span.line) + ":" + std::to_string(span.column) +
                             ": error: " + message + span.expansion} {}
};

enum class TokenKind {
    atom,
    keyword,
    string,
    left_parenthesis,
    right_parenthesis,
    end,
};

struct Token {
    TokenKind kind;
    std::string text;
    SourceSpan span;
};

}
