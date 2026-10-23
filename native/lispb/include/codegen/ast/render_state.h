#pragma once
#include <codegen/ast/environments.h>
#include <codegen/ast/node_environment_key.h>
#include <lispb/strings.h>

#include <span>
#include <string>
#include <unordered_map>

namespace lispb::codegen {
// Own lookup state for one render; indentation contexts only borrow it.
struct RenderState {
    explicit RenderState(Strings& strings)
        : strings{strings} {}

    Strings& strings;
    Environments environments;
    EnvironmentId current{environments.global()};
    std::unordered_map<NodeEnvironmentKey, EnvironmentId> node_environments;

    // A node can be emitted in more than one enclosing scope.
    static auto node_key(NodeId node, EnvironmentId parent) -> NodeEnvironmentKey {
        return (static_cast<std::uint64_t>(parent.value) << 32) | node.value;
    }

    void append_name(std::string& output, std::span<IdentifierId const> components);
};
}
