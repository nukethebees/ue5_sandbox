#pragma once

#include <codegen/sexpr/syntax.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace slate_codegen::detail {

using codegen::sexpr::SourceError;
using codegen::sexpr::SourceSpan;
using codegen::sexpr::Token;
using codegen::sexpr::TokenKind;

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

enum class DeclarationKind { widget_class, widget_library };

struct WidgetDeclaration {
    std::string name;
    std::vector<SlateFunction> functions;
    SourceSpan span;
    DeclarationKind kind{DeclarationKind::widget_class};
};

struct Document {
    std::vector<WidgetDeclaration> declarations;
};

}
