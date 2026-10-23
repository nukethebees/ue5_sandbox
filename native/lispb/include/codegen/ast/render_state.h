#pragma once

#include <codegen/ast/environments.h>
#include <codegen/ast/node_environment_key.h>

#include <unordered_map>

namespace lispb::codegen {
// Own lookup state for one emission; indentation contexts carry no lookup state.
struct RenderState {
    Environments environments;
    EnvironmentId current{environments.global()};
    std::unordered_map<NodeEnvironmentKey, EnvironmentId> node_environments;

    void clear();
    // A node can be emitted in more than one enclosing scope.
    static auto node_key(NodeId node, EnvironmentId parent) -> NodeEnvironmentKey {
        return (static_cast<NodeEnvironmentKey>(parent.value) << 32) | node.value;
    }
};
}
