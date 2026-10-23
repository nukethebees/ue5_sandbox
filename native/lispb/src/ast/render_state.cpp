#include <codegen/ast/render_state.h>

namespace lispb::codegen {
void RenderState::append_name(std::string& output, std::span<IdentifierId const> components) {
    std::string canonical;
    for (auto const component : components) {
        if (!canonical.empty()) {
            canonical += "::";
        }
        canonical += strings.view(component);
    }
    auto const target{environments.reference(components)};

    auto const count{components.size()};
    for (auto first{count - 1};; --first) {
        auto const result{environments.lookup(current, components.subspan(first))};
        if (result.status == LookupStatus::found && result.symbol.same_declaration(target)) {
            for (auto component{first}; component < count; ++component) {
                if (component != first) {
                    output += "::";
                }
                output += strings.view(components[component]);
            }
            return;
        }
        if (first == 0) {
            // Preserve authored qualification when the complete name cannot be checked.
            if (result.status != LookupStatus::unknown) {
                output += "::";
            }
            output += canonical;
            return;
        }
    }
}
}
