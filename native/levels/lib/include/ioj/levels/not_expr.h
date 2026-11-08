#pragma once
#include <ioj/levels/expr_index.h>
#include <ioj/levels/expr_type.h>

#include <vector>
namespace ioj::levels {
struct NotExpr {
    auto operator==(NotExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::Not};
    std::vector<ExprIndex> children;
};
}
