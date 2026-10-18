#include <codegen/ast/render_context.h>

#include <string>
#include <string_view>

namespace lispb::codegen {

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
