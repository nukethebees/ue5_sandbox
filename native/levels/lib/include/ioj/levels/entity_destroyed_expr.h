#pragma once

#include <ioj/levels/expr_type.h>
#include <ioj/levels/identifiers.h>

namespace ioj::levels {
struct EntityDestroyedExpr {
    auto operator==(EntityDestroyedExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::EntityDestroyed};
    EntityId entity{};
};
}
