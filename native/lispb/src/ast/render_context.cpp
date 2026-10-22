#include <codegen/ast/render_context.h>

#include <codegen/ast/cpp_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>

namespace lispb::codegen {

void CppTypes::append(std::string& output,
                      CppTypeId const type,
                      RenderContext const& context) const {
    assert(type.value != 0 && type.value <= shapes_.size());
    auto const shape{shapes_[type.value - 1]};
    if (shape.form == Form::raw || context.state == nullptr) {
        output += spelling(type);
    } else if (shape.form == Form::named) {
        if (shape.parts.count == 1) {
            output += spelling(type);
            return;
        }
        std::vector<std::string_view> components;
        for (auto const component :
             std::span{name_components_}.subspan(shape.parts.begin, shape.parts.count)) {
            components.push_back(strings_.view(component));
        }
        context.state->append_name(output, components);
    } else {
        for (auto const part : std::span{parts_}.subspan(shape.parts.begin, shape.parts.count)) {
            if (part.type.value != 0) {
                append(output, part.type, context);
            } else {
                output += strings_.view(part.text);
            }
        }
    }
}
void RenderState::append_name(std::string& output,
                              std::span<std::string_view const> components) const {
    auto append_from = [&](std::size_t first) {
        auto const count{components.size()};
        for (auto index{first}; index < count; ++index) {
            if (index != first) {
                output += "::";
            }
            output += components[index];
        }
    };

    auto common{std::size_t{}};
    auto const limit{std::min(namespaces.size(), components.size() - 1)};
    while (common < limit && namespaces[common] == components[common]) {
        ++common;
    }

    for (auto omitted{common};; --omitted) {
        bool hidden{};
        for (auto scope{scopes.rbegin()}; scope != scopes.rend(); ++scope) {
            if (scope->namespace_scope && scope->namespace_depth < omitted) {
                break;
            }
            // Authored declarations can introduce bindings that the IR cannot inspect.
            if (scope->opaque) {
                append_from(0);
                return;
            }
            if ((!scope->namespace_scope || scope->namespace_depth > omitted) &&
                std::ranges::find(scope->names, components[omitted]) != scope->names.end()) {
                hidden = true;
                break;
            }
        }
        if (!hidden) {
            append_from(omitted);
            return;
        }
        if (omitted == 0) {
            output += "::";
            append_from(0);
            return;
        }
    }
}

RenderScopeGuard::RenderScopeGuard(RenderState& state)
    : state_{state}
    , namespace_size_{state.namespaces.size()}
    , scope_size_{state.scopes.size()} {}
RenderScopeGuard::~RenderScopeGuard() {
    state_.namespaces.resize(namespace_size_);
    state_.scopes.resize(scope_size_);
}

namespace ir {
void append_indent(std::string& output, RenderContext const& context) {
    for (int level{}; level < context.indent_level; ++level) {
        output += context.indent_text;
    }
}
void append_indented(std::string& output, std::string_view text, RenderContext const& context) {
    bool line_start{true};
    for (auto const character : text) {
        if (line_start && character != '\n') {
            append_indent(output, context);
        }
        output += character;
        line_start = character == '\n';
    }
}
}

auto RenderContext::indent() const -> RenderContext {
    auto result{*this};
    ++result.indent_level;
    return result;
}
auto RenderContext::apply_indent(std::string_view text) const -> std::string {
    std::string result;
    ir::append_indented(result, text, *this);
    return result;
}

} // namespace lispb::codegen
