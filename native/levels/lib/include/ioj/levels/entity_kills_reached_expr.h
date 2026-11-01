#pragma once

#include <ioj/levels/expr_type.h>
#include <ioj/levels/identifiers.h>

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
