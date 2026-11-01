#pragma once

#include <ioj/levels/expr_type.h>
#include <ioj/levels/objective_dependency.h>

namespace ioj::levels {
struct ObjectiveResolvedExpr {
    auto operator==(ObjectiveResolvedExpr const&) const -> bool = default;

    static constexpr ExprType type{ExprType::ObjectiveResolved};
    ObjectiveDependency objective{};
};
}
