#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
struct EntityDestroyedExpression {
    static constexpr ExpressionType type{ExpressionType::EntityDestroyed};
    EntityId entity{};
    auto operator==(EntityDestroyedExpression const&) const -> bool = default;
};
}
