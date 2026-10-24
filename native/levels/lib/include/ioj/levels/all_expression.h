#pragma once

#include <ioj/levels/expression_index.h>
#include <ioj/levels/mission_types.h>

#include <vector>

namespace ioj::levels {
struct AllExpression {
    static constexpr ExpressionType type{ExpressionType::All};
    std::vector<ExpressionIndex> children{};
    auto operator==(AllExpression const&) const -> bool = default;
};
}
