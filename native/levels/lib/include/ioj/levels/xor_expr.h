#pragma once
#include <ioj/expr_kind.h>
#include <ioj/levels/expr_index.h>

#include <vector>
namespace ioj::levels {
struct XorExpr {
    auto operator==(XorExpr const&) const -> bool = default;
    static constexpr ExprKind type{ExprKind::Xor};
    std::vector<ExprIndex> children;
};
}
