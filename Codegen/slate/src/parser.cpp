#include "parser.h"

#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace slate_codegen::detail {
namespace {

class Parser {
  public:
    Parser(std::string_view const path, std::vector<Token> tokens)
        : path_{path}, tokens_{std::move(tokens)} {}

    auto parse() -> Document {
        Document document;
        std::set<std::string> owners;
        while (!at(TokenKind::end)) {
            if (!list_head_is("widget-class")) {
                fail(current().span, "expected 'widget-class' declaration");
            }
            auto widget_class{parse_widget_class()};
            if (!owners.insert(widget_class.owner).second) {
                fail(widget_class.span,
                     "duplicate widget class declaration '" + widget_class.owner + "'");
            }
            document.widget_classes.push_back(std::move(widget_class));
        }
        if (document.widget_classes.empty()) {
            fail(current().span, "document must contain at least one widget class");
        }
        return document;
    }

  private:
    struct ParameterState {
        ParameterKind kind;
        std::size_t uses{};
    };

    auto current() const -> Token const& { return tokens_[index_]; }

    auto previous() const -> Token const& { return tokens_[index_ - 1]; }

    auto at(TokenKind const kind) const -> bool { return current().kind == kind; }

    auto consume(TokenKind const kind) -> bool {
        if (!at(kind)) {
            return false;
        }
        ++index_;
        return true;
    }

    auto expect(TokenKind const kind, std::string_view const message) -> Token const& {
        if (!consume(kind)) {
            fail(current().span, message);
        }
        return previous();
    }

    auto expect_atom(std::string_view const message) -> Token const& {
        return expect(TokenKind::atom, message);
    }

    auto list_head_is(std::string_view const head) const -> bool {
        if (!at(TokenKind::left_parenthesis) || index_ + 1 >= tokens_.size()) {
            return false;
        }
        auto const& head_token{tokens_[index_ + 1]};
        return head_token.kind == TokenKind::atom && head_token.text == head;
    }

    [[noreturn]] void fail(SourceSpan const span, std::string_view const message) const {
        throw SourceError{path_, span, std::string{message}};
    }

    auto parse_widget_class() -> WidgetClass {
        auto const& opening{
            expect(TokenKind::left_parenthesis, "expected '(' before widget class")};
        auto const& form{expect_atom("expected 'widget-class'")};
        if (form.text != "widget-class") {
            fail(form.span, "expected 'widget-class'");
        }

        auto const& owner{expect_atom("expected widget class owner type")};
        if (!is_qualified_identifier(owner.text)) {
            fail(owner.span, "widget class owner must be a qualified C++ identifier");
        }

        WidgetClass result{.owner = owner.text, .span = opening.span};
        std::set<std::string> function_names;
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after widget class declaration");
            }
            if (!list_head_is("function")) {
                fail(current().span, "expected 'function' declaration");
            }
            auto function{parse_slate_function()};
            if (!function_names.insert(function.name).second) {
                fail(function.span, "duplicate generated function '" + function.name + "'");
            }
            result.functions.push_back(std::move(function));
        }
        expect(TokenKind::right_parenthesis, "expected ')' after widget class declaration");
        if (result.functions.empty()) {
            fail(opening.span, "widget class must contain at least one function");
        }
        return result;
    }

    auto parse_slate_function() -> SlateFunction {
        auto const& opening{
            expect(TokenKind::left_parenthesis, "expected '(' before function")};
        auto const& form{expect_atom("expected 'function'")};
        if (form.text != "function") {
            fail(form.span, "expected 'function'");
        }

        auto const& name{expect_atom("expected generated function name")};
        if (!is_identifier(name.text)) {
            fail(name.span, "generated function name must be a C++ identifier");
        }
        parameter_states_.clear();
        binding_names_.clear();
        auto parameters{parse_parameters()};
        std::vector<Binding> bindings;
        auto root{list_head_is("let") ? parse_let(bindings) : parse_child()};
        expect(TokenKind::right_parenthesis, "expected ')' after generated function");
        for (auto const& parameter : parameters) {
            if (parameter_states_.at(parameter.name).uses == 0) {
                fail(parameter.span, "unused " + parameter_kind_name(parameter.kind) +
                                         " parameter '" + parameter.name + "'");
            }
        }
        return SlateFunction{name.text,
                             std::move(parameters),
                             std::move(bindings),
                             std::move(root),
                             opening.span};
    }

    auto parse_parameters() -> std::vector<FunctionParameter> {
        if (!list_head_is("params")) {
            fail(current().span, "expected 'params' declaration after function name");
        }
        expect(TokenKind::left_parenthesis, "expected '(' before params");
        expect_atom("expected 'params'");

        std::vector<FunctionParameter> parameters;
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after params declaration");
            }
            auto const& opening{expect(TokenKind::left_parenthesis, "expected parameter declaration")};
            auto const& role{
                expect_atom("expected value, callback, factory, or existing parameter role")};
            auto kind{ParameterKind::value};
            if (role.text == "callback") {
                kind = ParameterKind::callback;
            } else if (role.text == "factory") {
                kind = ParameterKind::factory;
            } else if (role.text == "existing") {
                kind = ParameterKind::existing;
            } else if (role.text != "value") {
                fail(role.span, "expected value, callback, factory, or existing parameter role");
            }
            auto const& name{expect_atom("expected parameter name")};
            if (!is_identifier(name.text)) {
                fail(name.span, "parameter name must be a C++ identifier");
            }
            expect(TokenKind::right_parenthesis, "expected ')' after parameter declaration");
            if (name.text == "self_" || name.text == "ThisClass") {
                fail(name.span, "reserved generated name cannot be used as a parameter");
            }
            if (!parameter_states_.emplace(name.text, ParameterState{kind}).second) {
                fail(name.span, "duplicate parameter declaration '" + name.text + "'");
            }
            parameters.push_back(FunctionParameter{kind, name.text, opening.span});
        }
        expect(TokenKind::right_parenthesis, "expected ')' after params declaration");
        return parameters;
    }

    auto parse_let(std::vector<Binding>& bindings) -> Child {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected '(' before let")};
        auto const& form{expect_atom("expected 'let'")};
        if (form.text != "let") {
            fail(form.span, "expected 'let'");
        }
        expect(TokenKind::left_parenthesis, "expected '(' before let bindings");
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after let bindings");
            }
            bindings.push_back(parse_binding());
        }
        expect(TokenKind::right_parenthesis, "expected ')' after let bindings");
        if (bindings.empty()) {
            fail(opening.span, "let must contain at least one binding");
        }

        auto root{parse_child()};
        expect(TokenKind::right_parenthesis, "expected ')' after let body");
        return root;
    }

    auto parse_binding() -> Binding {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected let binding")};
        auto const& name{expect_atom("expected let binding name")};
        if (!is_identifier(name.text)) {
            fail(name.span, "let binding name must be a C++ identifier");
        }
        if (name.text == "self_" || name.text == "ThisClass") {
            fail(name.span, "reserved generated name cannot be used as a let binding");
        }
        if (binding_names_.contains(name.text)) {
            fail(name.span, "duplicate let binding '" + name.text + "'");
        }
        if (parameter_states_.contains(name.text)) {
            fail(name.span, "let binding '" + name.text + "' conflicts with parameter");
        }

        std::variant<Value, Margin> initializer;
        if (at(TokenKind::left_parenthesis) && !list_head_is("loc") &&
            !list_head_is("callback") && !list_head_is("method") && !list_head_is("uobject")) {
            initializer = parse_margin();
        } else {
            auto value{parse_value()};
            if (value.kind == ValueKind::callback || value.kind == ValueKind::method ||
                value.kind == ValueKind::uobject) {
                fail(value.span, "callable values cannot initialize let bindings");
            }
            initializer = std::move(value);
        }
        expect(TokenKind::right_parenthesis, "expected ')' after let binding");
        binding_names_.insert(name.text);
        return Binding{name.text, std::move(initializer), opening.span};
    }

    auto parse_widget() -> Child {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected '(' before widget")};
        auto const& type{expect_atom("expected widget type")};
        validate_widget_type(type);
        return parse_widget_body(opening.span, type.text, std::nullopt);
    }

    auto parse_assigned_widget() -> Child {
        auto const& opening{
            expect(TokenKind::left_parenthesis, "expected '(' before assigned widget")};
        auto const& form{expect_atom("expected 'assign'")};
        if (form.text != "assign") {
            fail(form.span, "expected 'assign'");
        }
        auto const& member{expect_atom("expected assigned widget member")};
        if (!is_identifier(member.text)) {
            fail(member.span, "assigned widget member must be a C++ identifier");
        }
        auto const& type{expect_atom("expected assigned widget type")};
        validate_widget_type(type);
        return parse_widget_body(opening.span, type.text, member.text);
    }

    void validate_widget_type(Token const& type) const {
        if (type.text == "widget-class" || type.text == "function" || type.text == "vbox" ||
            type.text == "hbox" || type.text == "slot" || type.text == "auto" ||
            type.text == "fill" || type.text == "assign" || type.text == "existing" ||
            type.text == "call" || type.text == "let" || type.text == "params") {
            fail(type.span, "expected widget type, found structural form '" + type.text + "'");
        }
    }

    auto parse_widget_body(SourceSpan const span,
                           std::string type,
                           std::optional<std::string> assigned_member) -> Child {
        Widget widget{.type = std::move(type),
                      .assigned_member = std::move(assigned_member),
                      .span = span};
        std::set<std::string> argument_names;
        std::set<std::string> slot_names;
        bool saw_child{false};

        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after widget body");
            }
            if (at(TokenKind::keyword)) {
                if (saw_child) {
                    fail(current().span, "widget arguments must appear before children");
                }
                auto argument{parse_argument()};
                if (!argument_names.insert(argument.name).second) {
                    fail(argument.span, "duplicate widget argument ':" + argument.name + "'");
                }
                widget.arguments.push_back(std::move(argument));
                continue;
            }
            if (!at(TokenKind::left_parenthesis)) {
                fail(current().span, "expected widget argument or child list");
            }

            saw_child = true;
            if (list_head_is("slot")) {
                auto slot{parse_named_slot()};
                if (!slot_names.insert(slot.name).second) {
                    fail(slot.span, "duplicate named slot '" + slot.name + "'");
                }
                widget.named_slots.push_back(std::move(slot));
                continue;
            }
            if (widget.content) {
                fail(current().span, "widget may contain only one default child");
            }
            widget.content_span = current().span;
            widget.content = std::make_shared<Child>(parse_child());
        }

        expect(TokenKind::right_parenthesis, "expected ')' after widget body");
        return Child{std::move(widget), span};
    }

    auto parse_argument() -> Argument {
        auto const& name{expect(TokenKind::keyword, "expected widget argument")};
        return Argument{name.text, parse_value(), name.span};
    }

    auto parse_value() -> Value {
        auto const span{current().span};
        if (consume(TokenKind::string)) {
            return Value{ValueKind::text, previous().text, span};
        }
        if (at(TokenKind::left_parenthesis)) {
            return parse_compound_value();
        }
        if (!consume(TokenKind::atom)) {
            fail(span, "expected number, boolean, text, or symbol");
        }

        auto const& token{previous()};
        if (token.text == "true" || token.text == "false") {
            return Value{ValueKind::boolean, token.text, span};
        }
        if (is_number(token.text)) {
            return Value{ValueKind::number, token.text, span};
        }
        if (is_identifier(token.text) && parameter_states_.contains(token.text)) {
            use_parameter(token.text, ParameterKind::value, token.span, true);
        }
        return Value{ValueKind::symbol, token.text, span};
    }

    auto parse_compound_value() -> Value {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected compound value")};
        auto const& form{expect_atom("expected callback, method, uobject, or loc")};
        if (form.text == "loc") {
            auto const& context{expect(TokenKind::string, "expected localization context")};
            auto const& key{expect(TokenKind::string, "expected localization key")};
            auto const& text{expect(TokenKind::string, "expected localized text")};
            expect(TokenKind::right_parenthesis, "expected ')' after localized text");
            return Value{.kind = ValueKind::localized_text,
                         .span = opening.span,
                         .localized_text = LocalizedText{context.text, key.text, text.text}};
        }

        auto kind{ValueKind::symbol};
        if (form.text == "callback") {
            kind = ValueKind::callback;
        } else if (form.text == "method") {
            kind = ValueKind::method;
        } else if (form.text == "uobject") {
            kind = ValueKind::uobject;
        } else {
            fail(form.span, "expected callback, method, uobject, or loc value form");
        }

        auto const& name{expect_atom("expected callable name")};
        if (!is_identifier(name.text)) {
            fail(name.span, "callable name must be a C++ identifier");
        }
        expect(TokenKind::right_parenthesis, "expected ')' after callable value");
        if (kind == ValueKind::callback) {
            use_parameter(name.text, ParameterKind::callback, opening.span, false);
        }
        return Value{kind, name.text, opening.span};
    }

    auto parse_existing_widget() -> Child {
        auto const& opening{
            expect(TokenKind::left_parenthesis, "expected '(' before existing widget")};
        auto const& form{expect_atom("expected 'existing'")};
        if (form.text != "existing") {
            fail(form.span, "expected 'existing'");
        }
        auto const& parameter{expect_atom("expected existing widget parameter")};
        if (!is_identifier(parameter.text)) {
            fail(parameter.span, "existing widget parameter must be a C++ identifier");
        }
        expect(TokenKind::right_parenthesis, "expected ')' after existing widget");
        use_parameter(parameter.text, ParameterKind::existing, opening.span, false);
        return Child{ExistingWidget{parameter.text, opening.span}, opening.span};
    }

    auto parse_called_widget() -> Child {
        auto const& opening{
            expect(TokenKind::left_parenthesis, "expected '(' before child factory call")};
        auto const& form{expect_atom("expected 'call'")};
        if (form.text != "call") {
            fail(form.span, "expected 'call'");
        }
        auto const& parameter{expect_atom("expected child factory parameter")};
        if (!is_identifier(parameter.text)) {
            fail(parameter.span, "child factory parameter must be a C++ identifier");
        }

        std::vector<Value> arguments;
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after child factory call");
            }
            auto value{parse_value()};
            if (value.kind == ValueKind::callback || value.kind == ValueKind::method ||
                value.kind == ValueKind::uobject) {
                fail(value.span, "callable forms cannot be child factory arguments");
            }
            arguments.push_back(std::move(value));
        }
        expect(TokenKind::right_parenthesis, "expected ')' after child factory call");
        use_parameter(parameter.text, ParameterKind::factory, opening.span, true);
        return Child{CalledWidget{parameter.text, std::move(arguments), opening.span}, opening.span};
    }

    void use_parameter(std::string const& name,
                       ParameterKind const expected_kind,
                       SourceSpan const span,
                       bool const allow_multiple_uses) {
        auto const found{parameter_states_.find(name)};
        if (found == parameter_states_.end()) {
            fail(span, "undeclared " + parameter_kind_name(expected_kind) + " parameter '" + name +
                           "'");
        }
        auto& state{found->second};
        if (state.kind != expected_kind) {
            fail(span, "parameter '" + name + "' is declared as " +
                           parameter_kind_name(state.kind) + ", not " +
                           parameter_kind_name(expected_kind));
        }
        if (!allow_multiple_uses && state.uses != 0) {
            fail(span, parameter_kind_name(expected_kind) + " parameter '" + name +
                           "' may only be used once");
        }
        ++state.uses;
    }

    static auto parameter_kind_name(ParameterKind const kind) -> std::string {
        if (kind == ParameterKind::value) {
            return "value";
        }
        if (kind == ParameterKind::callback) {
            return "callback";
        }
        if (kind == ParameterKind::factory) {
            return "factory";
        }
        return "existing";
    }

    auto parse_named_slot() -> WidgetSlot {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected '(' before named slot")};
        auto const& form{expect_atom("expected 'slot'")};
        if (form.text != "slot") {
            fail(form.span, "expected 'slot'");
        }
        auto const& name{expect_atom("expected named slot name")};
        auto child{std::make_shared<Child>(parse_child())};
        expect(TokenKind::right_parenthesis, "expected ')' after named slot");
        return WidgetSlot{name.text, std::move(child), opening.span};
    }

    auto parse_child() -> Child {
        if (!at(TokenKind::left_parenthesis)) {
            fail(current().span, "expected child list");
        }
        if (list_head_is("vbox") || list_head_is("hbox")) {
            return parse_box();
        }
        if (list_head_is("assign")) {
            return parse_assigned_widget();
        }
        if (list_head_is("existing")) {
            return parse_existing_widget();
        }
        if (list_head_is("call")) {
            return parse_called_widget();
        }
        if (list_head_is("widget-class") || list_head_is("function") || list_head_is("slot") ||
            list_head_is("auto") || list_head_is("fill")) {
            fail(tokens_[index_ + 1].span,
                 "expected widget, assigned widget, existing widget, or box child");
        }
        return parse_widget();
    }

    auto parse_box() -> Child {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected '(' before box")};
        auto const& form{expect_atom("expected 'vbox' or 'hbox'")};
        if (form.text != "vbox" && form.text != "hbox") {
            fail(form.span, "expected 'vbox' or 'hbox'");
        }

        Box box{.orientation = form.text == "vbox" ? BoxOrientation::vertical
                                                   : BoxOrientation::horizontal,
                .span = opening.span};
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after box body");
            }
            box.slots.push_back(parse_box_slot());
        }
        expect(TokenKind::right_parenthesis, "expected ')' after box body");
        if (box.slots.empty()) {
            fail(opening.span, "box must contain at least one slot");
        }
        return Child{std::move(box), opening.span};
    }

    auto parse_box_slot() -> BoxSlot {
        auto const& opening{expect(TokenKind::left_parenthesis, "expected box slot list")};
        auto const& mode{expect_atom("expected 'auto' or 'fill' box slot")};
        if (mode.text != "auto" && mode.text != "fill") {
            fail(mode.span, "expected 'auto' or 'fill' box slot");
        }

        BoxSlot slot{.fill = mode.text == "fill", .span = opening.span};
        bool has_padding{false};
        bool has_horizontal_alignment{false};
        bool has_vertical_alignment{false};

        while (at(TokenKind::keyword)) {
            auto const& option{expect(TokenKind::keyword, "expected box slot option")};
            if (option.text == "weight") {
                if (!slot.fill) {
                    fail(option.span, "weight is only valid on fill slots");
                }
                if (slot.weight) {
                    fail(option.span, "duplicate fill weight");
                }
                auto weight{parse_value()};
                if (weight.kind != ValueKind::number &&
                    (weight.kind != ValueKind::symbol ||
                     !parameter_states_.contains(weight.text))) {
                    fail(weight.span, "expected fill weight number or value parameter");
                }
                slot.weight = std::move(weight);
            } else if (option.text == "padding") {
                if (has_padding) {
                    fail(option.span, "duplicate slot padding");
                }
                has_padding = true;
                slot.padding = parse_margin();
            } else if (option.text == "halign") {
                if (has_horizontal_alignment) {
                    fail(option.span, "duplicate horizontal alignment");
                }
                has_horizontal_alignment = true;
                slot.horizontal_alignment = parse_alignment(true);
            } else if (option.text == "valign") {
                if (has_vertical_alignment) {
                    fail(option.span, "duplicate vertical alignment");
                }
                has_vertical_alignment = true;
                slot.vertical_alignment = parse_alignment(false);
            } else {
                fail(option.span, "unsupported box slot option ':" + option.text + "'");
            }
        }

        slot.child = std::make_shared<Child>(parse_child());
        expect(TokenKind::right_parenthesis, "expected ')' after box slot");
        return slot;
    }

    auto parse_number(std::string_view const message) -> std::string {
        auto const& value{expect_atom(message)};
        if (!is_number(value.text)) {
            fail(value.span, message);
        }
        return value.text;
    }

    auto parse_margin() -> Margin {
        if (!consume(TokenKind::left_parenthesis)) {
            auto const& value{expect_atom("expected padding value or let binding")};
            if (is_number(value.text)) {
                return Margin{.values = {value.text}};
            }
            if (!is_identifier(value.text)) {
                fail(value.span, "expected padding value or earlier let binding");
            }
            if (!binding_names_.contains(value.text)) {
                use_parameter(value.text, ParameterKind::value, value.span, true);
            }
            return Margin{.binding = value.text};
        }

        std::vector<std::string> values;
        while (!at(TokenKind::right_parenthesis)) {
            if (at(TokenKind::end)) {
                fail(current().span, "expected ')' after padding");
            }
            values.push_back(parse_number("expected padding value"));
        }
        auto const& closing{expect(TokenKind::right_parenthesis, "expected ')' after padding")};
        if (values.size() != 2 && values.size() != 4) {
            fail(closing.span, "padding tuple must contain two or four values");
        }
        return Margin{.values = std::move(values)};
    }

    auto parse_alignment(bool const horizontal) -> std::string {
        auto const& value{expect_atom("expected alignment value")};
        if (is_identifier(value.text) && parameter_states_.contains(value.text)) {
            use_parameter(value.text, ParameterKind::value, value.span, true);
            return value.text;
        }
        auto const valid{horizontal ? value.text == "left" || value.text == "center" ||
                                          value.text == "right" || value.text == "fill"
                                    : value.text == "top" || value.text == "center" ||
                                          value.text == "bottom" || value.text == "fill"};
        if (!valid) {
            fail(value.span,
                 horizontal ? "expected left, center, right, or fill"
                            : "expected top, center, bottom, or fill");
        }
        return value.text;
    }

    static auto is_number(std::string_view const text) -> bool {
        if (text.empty()) {
            return false;
        }

        std::size_t index{};
        if (text[index] == '-') {
            ++index;
        }
        if (index == text.size() || std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
            return false;
        }
        while (index < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        if (index < text.size() && text[index] == '.') {
            ++index;
            if (index == text.size() ||
                std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
                return false;
            }
            while (index < text.size() &&
                   std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                ++index;
            }
        }
        return index == text.size();
    }

    static auto is_identifier(std::string_view const text) -> bool {
        if (text.empty() ||
            (std::isalpha(static_cast<unsigned char>(text.front())) == 0 && text.front() != '_')) {
            return false;
        }
        for (auto const character : text.substr(1)) {
            if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_') {
                return false;
            }
        }
        return true;
    }

    static auto is_qualified_identifier(std::string_view text) -> bool {
        while (true) {
            auto const separator{text.find("::")};
            auto const component{text.substr(0, separator)};
            if (!is_identifier(component)) {
                return false;
            }
            if (separator == std::string_view::npos) {
                return true;
            }
            text.remove_prefix(separator + 2);
        }
    }

    std::string_view path_;
    std::vector<Token> tokens_;
    std::size_t index_{};
    std::map<std::string, ParameterState> parameter_states_;
    std::set<std::string> binding_names_;
};

}

auto parse(std::string_view const path, std::vector<Token> tokens) -> Document {
    return Parser{path, std::move(tokens)}.parse();
}

}
