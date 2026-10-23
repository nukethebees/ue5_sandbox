#include <codegen/ast/render_context.h>

namespace lispb::codegen {
auto RenderContext::indent() const -> RenderContext {
    auto result{*this};
    ++result.indent_level;
    return result;
}
}
