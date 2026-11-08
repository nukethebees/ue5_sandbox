#pragma once
#include <ioj/levels/expr_index.h>
#include <ioj/levels/expr_type.h>

#include <vector>
namespace ioj::levels {
struct XorExpr {
    auto operator==(XorExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::Xor};
    std::vector<ExprIndex> children;
};
}
