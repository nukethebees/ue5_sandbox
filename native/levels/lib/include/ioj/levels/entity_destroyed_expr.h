#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
struct EntityDestroyedExpr {
    auto operator==(EntityDestroyedExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::EntityDestroyed};
    EntityId entity{};
};
}
