#pragma once

#include <ioj/expr_kind.h>
#include <ioj/levels/identifiers.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
struct EntityKillsReachedExpr {
    auto operator==(EntityKillsReachedExpr const&) const -> bool = default;

    static constexpr ExprKind type{ExprKind::EntityKillsReached};
    std::vector<EntityId> entities{};
    std::uint32_t kills{};
};
}
