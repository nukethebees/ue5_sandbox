#pragma once

#include <ioj/levels/expression_index.h>
#include <ioj/levels/mission_types.h>

#include <vector>

namespace ioj::levels {
struct AnyExpression {
    static constexpr ExpressionType type{ExpressionType::Any};
    std::vector<ExpressionIndex> children{};
    auto operator==(AnyExpression const&) const -> bool = default;
};
}
