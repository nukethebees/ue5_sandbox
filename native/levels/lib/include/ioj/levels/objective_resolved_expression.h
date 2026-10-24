#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/objective_dependency.h>

namespace ioj::levels {
struct ObjectiveResolvedExpression {
    static constexpr ExpressionType type{ExpressionType::ObjectiveResolved};
    ObjectiveDependency objective{};
    auto operator==(ObjectiveResolvedExpression const&) const -> bool = default;
};
}
