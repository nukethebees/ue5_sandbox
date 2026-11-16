#pragma once

#include <ioj/expr_kind.h>
#include <ioj/levels/identifiers.h>

namespace ioj::levels {
struct EntityDestroyedExpr {
    auto operator==(EntityDestroyedExpr const&) const -> bool = default;

    static constexpr ExprKind type{ExprKind::EntityDestroyed};
    EntityId entity{};
};
}
