#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace slate_codegen::detail {

struct SourceSpan {
    std::size_t line{1};
    std::size_t column{1};
};

class SourceError final : public std::runtime_error {
  public:
    SourceError(std::string_view const path, SourceSpan const span, std::string const& message)
        : std::runtime_error{std::string{path} + ":" + std::to_string(span.line) + ":" +
                             std::to_string(span.column) + ": error: " + message} {}
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

enum class ValueKind { number, boolean, text, symbol };

struct Value {
    ValueKind kind;
    std::string text;
    SourceSpan span;
};

struct Argument {
    std::string name;
    Value value;
    SourceSpan span;
};

struct Child;

struct WidgetSlot {
    std::string name;
    std::shared_ptr<Child> child;
    SourceSpan span;
};

struct Widget {
    std::string type;
    std::vector<Argument> arguments;
    std::shared_ptr<Child> content;
    SourceSpan content_span;
    std::vector<WidgetSlot> named_slots;
    SourceSpan span;
};

struct BoxSlot {
    bool fill{false};
    std::optional<std::string> weight;
    std::vector<std::string> padding;
    std::optional<std::string> horizontal_alignment;
    std::optional<std::string> vertical_alignment;
    std::shared_ptr<Child> child;
    SourceSpan span;
};

struct VBox {
    std::vector<BoxSlot> slots;
    SourceSpan span;
};

struct Child {
    std::variant<Widget, VBox> value;
    SourceSpan span;
};

}
