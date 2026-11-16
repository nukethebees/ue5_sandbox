#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>
#include <ioj/expr_kind.h>
#include <ioj/levels/identifiers.h>

namespace ioj::levels {
struct EntityReachedPositionExpr {
    auto operator==(EntityReachedPositionExpr const&) const -> bool = default;

    static constexpr ExprKind type{ExprKind::EntityReachedPosition};
    EntityId entity{};
    FiniteVector<double, 3> position{};
    NonNegativeFinite<double> radius{};
};
}
