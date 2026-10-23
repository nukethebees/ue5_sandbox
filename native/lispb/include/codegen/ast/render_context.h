#pragma once

#include <codegen/ast/environments.h>
#include <lispb/strings.h>

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lispb::codegen {

// Own lookup state for one render; indentation contexts only borrow it.
struct RenderState {
    explicit RenderState(Strings& strings)
        : strings{strings} {}

    Strings& strings;
    Environments environments;
    EnvironmentId current{environments.global()};
    std::unordered_map<std::uint64_t, EnvironmentId> node_environments;

    // A node can be emitted in more than one enclosing scope.
    static auto node_key(NodeId node, EnvironmentId parent) -> std::uint64_t {
        return (static_cast<std::uint64_t>(parent.value) << 32) | node.value;
    }

    void append_name(std::string& output, std::span<IdentifierId const> components);
};

struct RenderContext {
    int indent_level{0};
    std::string_view indent_text{"    "};
    std::span<std::vector<std::string> const> include_groups;
    bool record_scope{false};
    RenderState* state{};

    auto indent() const -> RenderContext;
    auto apply_indent(std::string_view text) const -> std::string;
};

namespace ir {
void append_indent(std::string& output, RenderContext const& context);
void append_indented(std::string& output, std::string_view text, RenderContext const& context);
}

} // namespace lispb::codegen
