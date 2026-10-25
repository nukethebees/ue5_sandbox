#include "parser.h"

#include <ioj/is_any.h>

#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/diagnostics.h>
#include <lispb/indices.h>
#include <lispb/strings.h>
#include <slate_codegen/alignment.h>
#include <slate_codegen/alignment_kind.h>
#include <slate_codegen/box_orientation.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/child_kind.h>
#include <slate_codegen/declaration_kind.h>
#include <slate_codegen/document.h>
#include <slate_codegen/parameter_kind.h>
#include <slate_codegen/range.h>
#include <slate_codegen/value_id.h>
#include <slate_codegen/value_kind.h>

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace slate_codegen::detail {
namespace {

using lispb::StringId;
using lispb::codegen::sexpr::FormId;
using lispb::codegen::sexpr::HandleRange;
using lispb::codegen::sexpr::SourceSpan;
using lispb::codegen::sexpr::Syntax;
using lispb::codegen::sexpr::TokenKind;

auto is_identifier(std::string_view const text) -> bool {
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

auto is_qualified_identifier(std::string_view text) -> bool {
    while (true) {
        auto const separator{text.find("::")};
        if (!is_identifier(text.substr(0, separator))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        text.remove_prefix(separator + 2);
    }
}

auto is_number(std::string_view const text) -> bool {
    if (text.empty()) {
        return false;
    }
    std::size_t index{text.front() == '-' ? 1u : 0u};
    if (index == text.size() || std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
        return false;
    }
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    if (index < text.size() && text[index] == '.') {
        ++index;
        auto const begin{index};
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        if (index == begin) {
            return false;
        }
    }
    return index == text.size();
}

auto child_id(ChildKind const kind, std::size_t const index) -> ChildId {
    assert(index < 0x01000000u);
    return {static_cast<std::uint32_t>(kind) << 24 | static_cast<std::uint32_t>(index)};
}
auto value_id(ValueKind const kind, std::size_t const index) -> ValueId {
    assert(index < 0x01000000u);
    return {static_cast<std::uint32_t>(kind) << 24 | static_cast<std::uint32_t>(index)};
}
auto start(std::size_t const size) -> Range {
    return {static_cast<std::uint32_t>(size), 0};
}
void finish(Range& range, std::size_t const size) {
    range.count = static_cast<std::uint32_t>(size) - range.begin;
}

class Parser {
  public:
    Parser(Syntax& syntax, Document& document)
        : syntax_{syntax}
        , document_{document} {}

    void parse(HandleRange const forms) {
        if (forms.count == 0) {
            fail({}, "document must contain at least one widget class or library");
        }
        std::set<StringId> owners{document_.declarations.names.begin(),
                                  document_.declarations.names.end()};
        for (auto const form : syntax_.forms(forms)) {
            if (syntax_.head(form) != "widget-class" && syntax_.head(form) != "widget-library") {
                fail(syntax_.token_span(form),
                     "expected 'widget-class' or 'widget-library' declaration");
            }
            require_count(form, 3, "widget declaration must contain at least one function", false);
            auto const owner{
                atom(syntax_.children(form)[1], "expected qualified widget declaration name")};
            if (!is_qualified_identifier(syntax_.text(owner))) {
                fail(syntax_.token_span(owner),
                     "widget declaration name must be a qualified C++ identifier");
            }
            if (!owners.insert(syntax_.identifier(owner)).second) {
                fail(syntax_.token_span(owner),
                     std::format("duplicate widget declaration '{}'", syntax_.text(owner)));
            }
            library_ = syntax_.head(form) == "widget-library";
            auto functions{start(document_.functions.names.size())};
            std::set<StringId> function_names;
            auto const count{syntax_.children(form).size()};
            for (std::size_t index{2}; index < count; ++index) {
                auto const function{syntax_.children(form)[index]};
                if (syntax_.head(function) != "function") {
                    fail(syntax_.token_span(function), "expected 'function' declaration");
                }
                parse_function(function);
                auto const name{document_.functions.names.back()};
                if (!function_names.insert(name).second) {
                    fail(syntax_.token_span(function),
                         std::format("duplicate generated function '{}'",
                                     syntax_.strings().view(name)));
                }
            }
            finish(functions, document_.functions.names.size());
            document_.declarations.names.push_back(syntax_.identifier(owner));
            document_.declarations.functions.push_back(functions);
            document_.declarations.kinds.push_back(library_ ? DeclarationKind::widget_library
                                                            : DeclarationKind::widget_class);
            document_.declarations.spans.push_back(syntax_.token_span(form));
        }
    }
  private:
    struct ParameterState {
        ParameterKind kind;
        std::size_t uses{};
    };
    struct Argument {
        StringId name;
        ValueId value;
    };
    struct NamedSlot {
        StringId name;
        ChildId child;
    };
    struct BoxSlot {
        bool fill{};
        ValueId weight{};
        ValueId padding{};
        Alignment horizontal{};
        Alignment vertical{};
        ChildId child{};
    };

    [[noreturn]] void fail(SourceSpan const span, std::string_view const message) const {
        syntax_.sources().diagnostics().fail(syntax_.sources(), span, std::string{message});
    }
    void require_count(FormId const form,
                       std::size_t const count,
                       std::string_view const message,
                       bool const exact = true) const {
        if (!syntax_.is_list(form) || (exact ? syntax_.children(form).size() != count
                                             : syntax_.children(form).size() < count)) {
            fail(syntax_.token_span(form), message);
        }
    }
    auto atom(FormId const form, std::string_view const message) const -> FormId {
        if (syntax_.kind(form) != TokenKind::atom) {
            fail(syntax_.token_span(form), message);
        }
        return form;
    }
    auto string(FormId const form, std::string_view const message) const -> StringId {
        if (syntax_.kind(form) != TokenKind::string) {
            fail(syntax_.token_span(form), message);
        }
        return syntax_.text_id(form);
    }
    void validate_name(FormId const token, std::string_view const subject) const {
        if (!is_identifier(syntax_.text(token))) {
            fail(syntax_.token_span(token), std::format("{} must be a C++ identifier", subject));
        }
    }

    void parse_function(FormId const form) {
        require_count(form, 4, "expected 'params' declaration after function name");
        auto const name{atom(syntax_.children(form)[1], "expected generated function name")};
        validate_name(name, "generated function name");
        parameter_states_.clear();
        binding_names_.clear();
        auto const parameter_form{syntax_.children(form)[2]};
        if (syntax_.head(parameter_form) != "params") {
            fail(syntax_.token_span(parameter_form),
                 "expected 'params' declaration after function name");
        }
        auto parameters{start(document_.parameters.names.size())};
        auto const count{syntax_.children(parameter_form).size()};
        for (std::size_t index{1}; index < count; ++index) {
            parse_parameter(syntax_.children(parameter_form)[index]);
        }
        finish(parameters, document_.parameters.names.size());
        auto bindings{start(document_.bindings.names.size())};
        auto const body{syntax_.children(form)[3]};
        auto const root{syntax_.head(body) == "let" ? parse_let(body) : parse_child(body)};
        finish(bindings, document_.bindings.names.size());
        auto const end{parameters.begin + parameters.count};
        for (auto index{parameters.begin}; index < end; ++index) {
            auto const parameter{document_.parameters.names[index]};
            if (parameter_states_.at(parameter).uses == 0) {
                fail(document_.parameters.spans[index],
                     std::format("unused {} parameter '{}'",
                                 to_serialized_string(document_.parameters.kinds[index]),
                                 syntax_.strings().view(parameter)));
            }
        }
        document_.functions.names.push_back(syntax_.identifier(name));
        document_.functions.parameters.push_back(parameters);
        document_.functions.bindings.push_back(bindings);
        document_.functions.roots.push_back(root);
        document_.functions.spans.push_back(syntax_.token_span(form));
    }

    void parse_parameter(FormId const form) {
        require_count(form, 2, "expected parameter declaration");
        auto const role{atom(syntax_.children(form)[0],
                             "expected value, callback, factory, or existing parameter role")};
        auto const kind{try_parse_serialized_parameter_kind(syntax_.text(role))};
        if (!kind) {
            fail(syntax_.token_span(role),
                 "expected value, callback, factory, or existing parameter role");
        }
        auto const name{atom(syntax_.children(form)[1], "expected parameter name")};
        validate_name(name, "parameter name");
        if (syntax_.text(name) == "self_" || syntax_.text(name) == "ThisClass") {
            fail(syntax_.token_span(name), "reserved generated name cannot be used as a parameter");
        }
        if (!parameter_states_.emplace(syntax_.identifier(name), ParameterState{*kind}).second) {
            fail(syntax_.token_span(name),
                 std::format("duplicate parameter declaration '{}'", syntax_.text(name)));
        }
        document_.parameters.kinds.push_back(*kind);
        document_.parameters.names.push_back(syntax_.identifier(name));
        document_.parameters.spans.push_back(syntax_.token_span(form));
    }

    auto parse_let(FormId const form) -> ChildId {
        require_count(form, 3, "expected bindings and body after let");
        auto const bindings{syntax_.children(form)[1]};
        if (!syntax_.is_list(bindings) || syntax_.children(bindings).empty()) {
            fail(syntax_.token_span(bindings), "let must contain at least one binding");
        }
        for (auto const binding : syntax_.children(bindings)) {
            require_count(binding, 2, "expected let binding");
            auto const name{atom(syntax_.children(binding)[0], "expected let binding name")};
            validate_name(name, "let binding name");
            if (syntax_.text(name) == "self_" || syntax_.text(name) == "ThisClass") {
                fail(syntax_.token_span(name),
                     "reserved generated name cannot be used as a let binding");
            }
            if (binding_names_.contains(syntax_.identifier(name))) {
                fail(syntax_.token_span(name),
                     std::format("duplicate let binding '{}'", syntax_.text(name)));
            }
            if (parameter_states_.contains(syntax_.identifier(name))) {
                fail(syntax_.token_span(name),
                     std::format("let binding '{}' conflicts with parameter", syntax_.text(name)));
            }
            auto const initializer{syntax_.children(binding)[1]};
            auto const head{syntax_.head(initializer)};
            auto const value{syntax_.is_list(initializer) && head != "loc" && head != "callback" &&
                                     head != "method" && head != "uobject"
                                 ? parse_margin(initializer)
                                 : parse_value(initializer)};
            if (ioj::is_any(value.kind(),
                            {ValueKind::callback, ValueKind::method, ValueKind::uobject})) {
                fail(syntax_.token_span(initializer),
                     "callable values cannot initialize let bindings");
            }
            document_.bindings.names.push_back(syntax_.identifier(name));
            document_.bindings.initializers.push_back(value);
            document_.bindings.spans.push_back(syntax_.token_span(binding));
            binding_names_.insert(syntax_.identifier(name));
        }
        return parse_child(syntax_.children(form)[2]);
    }

    void use_parameter(FormId const name, ParameterKind const expected, bool const multiple) {
        auto const found{parameter_states_.find(syntax_.identifier(name))};
        auto const spelling{syntax_.text(name)};
        if (found == parameter_states_.end()) {
            fail(syntax_.token_span(name),
                 std::format(
                     "undeclared {} parameter '{}'", to_serialized_string(expected), spelling));
        }
        auto& state{found->second};
        if (state.kind != expected) {
            fail(syntax_.token_span(name),
                 std::format("parameter '{}' is declared as {}, not {}",
                             spelling,
                             to_serialized_string(state.kind),
                             to_serialized_string(expected)));
        }
        if (!multiple && state.uses != 0) {
            fail(syntax_.token_span(name),
                 std::format("{} parameter '{}' may only be used once",
                             to_serialized_string(expected),
                             spelling));
        }
        ++state.uses;
    }

    auto plain_value(ValueKind const kind, FormId const token) -> ValueId {
        auto const id{value_id(kind, document_.values.text.size())};
        document_.values.text.push_back(syntax_.text_id(token));
        document_.values.spans.push_back(syntax_.token_span(token));
        return id;
    }

    auto parse_value(FormId const form) -> ValueId {
        auto const token{form};
        if (syntax_.kind(token) == TokenKind::string) {
            return plain_value(ValueKind::text, token);
        }
        if (syntax_.is_list(form)) {
            require_count(form, 2, "expected callback, method, uobject, or loc", false);
            auto const head{syntax_.head(form)};
            if (library_ && (head == "method" || head == "uobject")) {
                fail(syntax_.token_span(form),
                     std::format("{} requires a widget-class host; unavailable in widget-library",
                                 head));
            }
            if (head == "loc") {
                require_count(form, 4, "expected localization context, key, and text");
                auto const context{
                    string(syntax_.children(form)[1], "expected localization context")};
                auto const key{string(syntax_.children(form)[2], "expected localization key")};
                auto const text{string(syntax_.children(form)[3], "expected localized text")};
                auto const id{value_id(ValueKind::localized_text, document_.localized.text.size())};
                document_.localized.contexts.push_back(context);
                document_.localized.keys.push_back(key);
                document_.localized.text.push_back(text);
                document_.localized.spans.push_back(syntax_.token_span(token));
                return id;
            }
            require_count(form, 2, "expected ')' after callable value");
            auto kind{ValueKind::callback};
            if (head == "method") {
                kind = ValueKind::method;
            } else if (head == "uobject") {
                kind = ValueKind::uobject;
            } else if (head != "callback") {
                fail(syntax_.token_span(token),
                     "expected callback, method, uobject, or loc value form");
            }
            auto const name{atom(syntax_.children(form)[1], "expected callable name")};
            validate_name(name, "callable name");
            if (kind == ValueKind::callback) {
                use_parameter(name, ParameterKind::callback, false);
            }
            return plain_value(kind, name);
        }
        atom(form, "expected number, boolean, text, or symbol");
        if (syntax_.text(token) == "true" || syntax_.text(token) == "false") {
            return plain_value(ValueKind::boolean, token);
        }
        if (is_number(syntax_.text(token))) {
            return plain_value(ValueKind::number, token);
        }
        if (is_identifier(syntax_.text(token)) &&
            parameter_states_.contains(syntax_.identifier(token))) {
            use_parameter(token, ParameterKind::value, true);
        }
        return plain_value(ValueKind::symbol, token);
    }

    auto parse_margin(FormId const form) -> ValueId {
        auto values{start(document_.margin_values.size())};
        StringId binding{};
        if (syntax_.is_list(form)) {
            if (syntax_.children(form).size() != 2 && syntax_.children(form).size() != 4) {
                fail(syntax_.closing_span(form), "padding tuple must contain two or four values");
            }
            for (auto const item : syntax_.children(form)) {
                auto const token{atom(item, "expected padding value")};
                if (!is_number(syntax_.text(token))) {
                    fail(syntax_.token_span(token), "expected padding value");
                }
                document_.margin_values.push_back(syntax_.text_id(token));
            }
        } else {
            auto const token{atom(form, "expected padding value or let binding")};
            if (is_number(syntax_.text(token))) {
                document_.margin_values.push_back(syntax_.text_id(token));
            } else {
                if (!is_identifier(syntax_.text(token))) {
                    fail(syntax_.token_span(token),
                         "expected padding value or earlier let binding");
                }
                if (!binding_names_.contains(syntax_.identifier(token))) {
                    use_parameter(token, ParameterKind::value, true);
                }
                binding = syntax_.identifier(token);
            }
        }
        finish(values, document_.margin_values.size());
        auto const id{value_id(ValueKind::margin, document_.margins.values.size())};
        document_.margins.values.push_back(values);
        document_.margins.bindings.push_back(binding);
        document_.margins.spans.push_back(syntax_.token_span(form));
        return id;
    }

    auto parse_alignment(FormId const form, bool const horizontal) -> Alignment {
        auto const token{atom(form, "expected alignment value")};
        if (is_identifier(syntax_.text(token)) &&
            parameter_states_.contains(syntax_.identifier(token))) {
            use_parameter(token, ParameterKind::value, true);
            return {syntax_.identifier(token), AlignmentKind::parameter};
        }
        if (syntax_.text(token) == (horizontal ? "left" : "top")) {
            return {{}, AlignmentKind::start};
        }
        if (syntax_.text(token) == (horizontal ? "right" : "bottom")) {
            return {{}, AlignmentKind::end};
        }
        if (syntax_.text(token) == "center") {
            return {{}, AlignmentKind::center};
        }
        if (syntax_.text(token) == "fill") {
            return {{}, AlignmentKind::fill};
        }
        fail(syntax_.token_span(token),
             horizontal ? "expected left, center, right, or fill"
                        : "expected top, center, bottom, or fill");
    }

    auto parse_child(FormId const form) -> ChildId {
        if (!syntax_.is_list(form) || syntax_.children(form).empty()) {
            fail(syntax_.token_span(form), "expected child list");
        }
        auto const head{syntax_.head(form)};
        if (head == "hbox" || head == "vbox") {
            return parse_box(form);
        }
        if (head == "existing" || head == "call") {
            require_count(form, 2, "expected widget parameter", head == "existing");
            auto const parameter{atom(syntax_.children(form)[1], "expected widget parameter")};
            validate_name(parameter, "widget parameter");
            if (head == "existing") {
                use_parameter(parameter, ParameterKind::existing, false);
                auto const id{child_id(ChildKind::existing, document_.existing.parameters.size())};
                document_.existing.parameters.push_back(syntax_.identifier(parameter));
                document_.existing.spans.push_back(syntax_.token_span(form));
                return id;
            }
            use_parameter(parameter, ParameterKind::factory, true);
            std::vector<ValueId> arguments;
            auto const count{syntax_.children(form).size()};
            for (std::size_t index{2}; index < count; ++index) {
                auto const value{parse_value(syntax_.children(form)[index])};
                if (ioj::is_any(value.kind(),
                                {ValueKind::callback, ValueKind::method, ValueKind::uobject})) {
                    fail(syntax_.token_span(syntax_.children(form)[index]),
                         "callable forms cannot be child factory arguments");
                }
                arguments.push_back(value);
            }
            auto range{start(document_.value_lists.size())};
            document_.value_lists.insert(
                document_.value_lists.end(), arguments.begin(), arguments.end());
            finish(range, document_.value_lists.size());
            auto const id{child_id(ChildKind::call, document_.calls.parameters.size())};
            document_.calls.parameters.push_back(syntax_.identifier(parameter));
            document_.calls.arguments.push_back(range);
            document_.calls.spans.push_back(syntax_.token_span(form));
            return id;
        }
        if (head == "widget-class" || head == "widget-library" || head == "function" ||
            head == "slot" || head == "auto" || head == "fill" || head == "let" ||
            head == "params") {
            fail(syntax_.token_span(form),
                 "expected widget, assigned widget, existing widget, or box child");
        }
        return parse_widget(form);
    }

    auto parse_widget(FormId const form) -> ChildId {
        std::size_t index{1};
        auto type{atom(syntax_.children(form)[0], "expected widget type")};
        StringId assigned{};
        if (syntax_.head(form) == "assign") {
            if (library_) {
                fail(syntax_.token_span(form),
                     "assign requires a widget-class host; unavailable in widget-library");
            }
            require_count(form, 3, "expected assigned widget member and type", false);
            auto const member{atom(syntax_.children(form)[1], "expected assigned widget member")};
            validate_name(member, "assigned widget member");
            assigned = syntax_.identifier(member);
            type = atom(syntax_.children(form)[2], "expected assigned widget type");
            index = 3;
        }
        auto const type_name{syntax_.text(type)};
        if (ioj::is_any<std::string_view>(type_name,
                                          {"widget-class",
                                           "widget-library",
                                           "function",
                                           "vbox",
                                           "hbox",
                                           "slot",
                                           "auto",
                                           "fill",
                                           "assign",
                                           "existing",
                                           "call",
                                           "let",
                                           "params"})) {
            fail(syntax_.token_span(type),
                 std::format("expected widget type, found structural form '{}'", type_name));
        }
        std::vector<Argument> arguments;
        std::vector<NamedSlot> slots;
        std::set<StringId> argument_names;
        std::set<StringId> slot_names;
        ChildId content{};
        bool saw_child{};
        auto const count{syntax_.children(form).size()};
        while (index < count) {
            auto const item{syntax_.children(form)[index++]};
            if (syntax_.kind(item) == TokenKind::keyword) {
                if (saw_child) {
                    fail(syntax_.token_span(item), "widget arguments must appear before children");
                }
                if (!argument_names.insert(syntax_.identifier(item)).second) {
                    fail(syntax_.token_span(item),
                         std::format("duplicate widget argument ':{}'", syntax_.text(item)));
                }
                if (index == count) {
                    fail(syntax_.token_span(item), "expected widget argument value");
                }
                arguments.push_back(
                    {syntax_.identifier(item), parse_value(syntax_.children(form)[index++])});
                continue;
            }
            saw_child = true;
            if (syntax_.head(item) == "slot") {
                require_count(item, 3, "expected named slot name and child");
                auto const name{atom(syntax_.children(item)[1], "expected named slot name")};
                if (!slot_names.insert(syntax_.identifier(name)).second) {
                    fail(syntax_.token_span(name),
                         std::format("duplicate named slot '{}'", syntax_.text(name)));
                }
                slots.push_back({syntax_.identifier(name), parse_child(syntax_.children(item)[2])});
            } else {
                if (content) {
                    fail(syntax_.token_span(item), "widget may contain only one default child");
                }
                content = parse_child(item);
            }
        }
        auto argument_range{start(document_.arguments.names.size())};
        for (auto const argument : arguments) {
            document_.arguments.names.push_back(argument.name);
            document_.arguments.values.push_back(argument.value);
        }
        finish(argument_range, document_.arguments.names.size());
        auto slot_range{start(document_.named_slots.names.size())};
        for (auto const slot : slots) {
            document_.named_slots.names.push_back(slot.name);
            document_.named_slots.children.push_back(slot.child);
        }
        finish(slot_range, document_.named_slots.names.size());
        auto const id{child_id(ChildKind::widget, document_.widgets.types.size())};
        document_.widgets.types.push_back(syntax_.identifier(type));
        document_.widgets.assigned_members.push_back(assigned);
        document_.widgets.arguments.push_back(argument_range);
        document_.widgets.contents.push_back(content);
        document_.widgets.named_slots.push_back(slot_range);
        document_.widgets.spans.push_back(syntax_.token_span(form));
        return id;
    }

    auto parse_box(FormId const form) -> ChildId {
        require_count(form, 2, "box must contain at least one slot", false);
        std::vector<BoxSlot> slots;
        auto const count{syntax_.children(form).size()};
        for (std::size_t index{1}; index < count; ++index) {
            slots.push_back(parse_box_slot(syntax_.children(form)[index]));
        }
        auto range{start(document_.box_slots.children.size())};
        for (auto const slot : slots) {
            document_.box_slots.fill.push_back(slot.fill);
            document_.box_slots.weights.push_back(slot.weight);
            document_.box_slots.padding.push_back(slot.padding);
            document_.box_slots.horizontal.push_back(slot.horizontal);
            document_.box_slots.vertical.push_back(slot.vertical);
            document_.box_slots.children.push_back(slot.child);
        }
        finish(range, document_.box_slots.children.size());
        auto const id{child_id(ChildKind::box, document_.boxes.slots.size())};
        document_.boxes.orientations.push_back(
            syntax_.head(form) == "vbox" ? BoxOrientation::vertical : BoxOrientation::horizontal);
        document_.boxes.slots.push_back(range);
        document_.boxes.spans.push_back(syntax_.token_span(form));
        return id;
    }

    auto parse_box_slot(FormId const form) -> BoxSlot {
        require_count(form, 2, "expected box slot list", false);
        if (syntax_.head(form) != "auto" && syntax_.head(form) != "fill") {
            fail(syntax_.token_span(form), "expected 'auto' or 'fill' box slot");
        }
        BoxSlot slot{.fill = syntax_.head(form) == "fill"};
        std::size_t index{1};
        auto const count{syntax_.children(form).size()};
        while (index < count && syntax_.kind(syntax_.children(form)[index]) == TokenKind::keyword) {
            auto const option{syntax_.children(form)[index++]};
            if (index == count) {
                fail(syntax_.token_span(option), "expected box slot option value");
            }
            auto const value{syntax_.children(form)[index++]};
            if (syntax_.text(option) == "weight") {
                if (!slot.fill) {
                    fail(syntax_.token_span(option), "weight is only valid on fill slots");
                }
                if (slot.weight) {
                    fail(syntax_.token_span(option), "duplicate fill weight");
                }
                slot.weight = parse_value(value);
                if (slot.weight.kind() != ValueKind::number &&
                    (slot.weight.kind() != ValueKind::symbol ||
                     !parameter_states_.contains(syntax_.identifier(value)))) {
                    fail(syntax_.token_span(value),
                         "expected fill weight number or value parameter");
                }
            } else if (syntax_.text(option) == "padding") {
                if (slot.padding) {
                    fail(syntax_.token_span(option), "duplicate slot padding");
                }
                slot.padding = parse_margin(value);
            } else if (syntax_.text(option) == "halign") {
                if (slot.horizontal.kind != AlignmentKind::none) {
                    fail(syntax_.token_span(option), "duplicate horizontal alignment");
                }
                slot.horizontal = parse_alignment(value, true);
            } else if (syntax_.text(option) == "valign") {
                if (slot.vertical.kind != AlignmentKind::none) {
                    fail(syntax_.token_span(option), "duplicate vertical alignment");
                }
                slot.vertical = parse_alignment(value, false);
            } else {
                fail(syntax_.token_span(option),
                     std::format("unsupported box slot option ':{}'", syntax_.text(option)));
            }
        }
        if (index + 1 != count) {
            fail(syntax_.token_span(form), "expected one child after box slot options");
        }
        slot.child = parse_child(syntax_.children(form)[index]);
        return slot;
    }

    Syntax& syntax_;
    Document& document_;
    bool library_{};
    std::map<StringId, ParameterState> parameter_states_;
    std::set<StringId> binding_names_;
};

}

void parse(Syntax& syntax, HandleRange const forms, Document& document) {
    Parser{syntax, document}.parse(forms);
}

}
