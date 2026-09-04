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

enum class ValueKind { number, boolean, text, localized_text, symbol, callback, method, uobject };

struct LocalizedText {
    std::string context;
    std::string key;
    std::string text;
};

struct Value {
    ValueKind kind;
    std::string text;
    SourceSpan span;
    std::optional<LocalizedText> localized_text;
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
    std::optional<std::string> assigned_member;
    std::vector<Argument> arguments;
    std::shared_ptr<Child> content;
    SourceSpan content_span;
    std::vector<WidgetSlot> named_slots;
    SourceSpan span;
};

struct ExistingWidget {
    std::string parameter;
    SourceSpan span;
};

struct CalledWidget {
    std::string parameter;
    std::vector<Value> arguments;
    SourceSpan span;
};

struct Margin {
    std::vector<std::string> values;
    std::optional<std::string> binding;
};

struct Binding {
    std::string name;
    std::variant<Value, Margin> initializer;
    SourceSpan span;
};

struct BoxSlot {
    bool fill{false};
    std::optional<Value> weight;
    std::optional<Margin> padding;
    std::optional<std::string> horizontal_alignment;
    std::optional<std::string> vertical_alignment;
    std::shared_ptr<Child> child;
    SourceSpan span;
};

enum class BoxOrientation { horizontal, vertical };

struct Box {
    BoxOrientation orientation;
    std::vector<BoxSlot> slots;
    SourceSpan span;
};

struct Child {
    std::variant<Widget, Box, ExistingWidget, CalledWidget> value;
    SourceSpan span;
};

enum class ParameterKind { value, callback, factory, existing };

struct FunctionParameter {
    ParameterKind kind;
    std::string name;
    SourceSpan span;
};

struct SlateFunction {
    std::string name;
    std::vector<FunctionParameter> parameters;
    std::vector<Binding> bindings;
    Child root;
    SourceSpan span;
};

struct WidgetClass {
    std::string owner;
    std::vector<SlateFunction> functions;
    SourceSpan span;
};

struct Document {
    std::vector<WidgetClass> widget_classes;
};

}
