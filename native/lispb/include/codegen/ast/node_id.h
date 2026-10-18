#pragma once

#include <codegen/ast/node_kind.h>

#include <cstdint>

namespace lispb::codegen {

struct NodeId {
    std::uint32_t value{};
    auto kind() const -> NodeKind { return static_cast<NodeKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto operator==(NodeId const&) const -> bool = default;
};

static_assert(sizeof(NodeId) == 4);

} // namespace lispb::codegen
