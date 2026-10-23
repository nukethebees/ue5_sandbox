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
        std::vector<IdentifierId> components;
        for (auto const component :
             std::span{name_components_}.subspan(shape.parts.begin, shape.parts.count)) {
            components.push_back(IdentifierId{component.value});
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
