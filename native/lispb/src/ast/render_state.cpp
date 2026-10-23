#include <codegen/ast/render_state.h>

namespace lispb::codegen {
void RenderState::clear() {
    environments.clear();
    current = environments.global();
    node_environments.clear();
}
}
