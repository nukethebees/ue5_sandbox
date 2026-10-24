#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
struct EntityKillsReachedExpr {
    auto operator==(EntityKillsReachedExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::EntityKillsReached};
    std::vector<EntityId> entities{};
    std::uint32_t kills{};
};
}
