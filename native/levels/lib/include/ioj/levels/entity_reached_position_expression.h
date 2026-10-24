#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
struct EntityReachedPositionExpression {
    static constexpr ExpressionType type{ExpressionType::EntityReachedPosition};
    EntityId entity{};
    FiniteVector<double, 3> position{};
    NonNegativeFinite<double> radius{};
    auto operator==(EntityReachedPositionExpression const&) const -> bool = default;
};
}
