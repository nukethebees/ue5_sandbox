#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
struct EntityKillsReachedExpression {
    static constexpr ExpressionType type{ExpressionType::EntityKillsReached};
    std::vector<EntityId> entities{};
    std::uint32_t kills{};
    auto operator==(EntityKillsReachedExpression const&) const -> bool = default;
};
}
