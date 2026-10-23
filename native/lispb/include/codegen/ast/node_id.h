#pragma once

#include <codegen/ast/node_kind.h>

#include <cstdint>

namespace lispb::codegen {

struct NodeId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    auto kind() const -> NodeKind { return static_cast<NodeKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator==(NodeId const&) const -> bool = default;
};

static_assert(sizeof(NodeId) == 4);

} // namespace lispb::codegen
