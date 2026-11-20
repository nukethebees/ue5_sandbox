#pragma once
#include <ioj/expr_kind.h>
#include <ioj/levels/logic_indices.h>

#include <vector>
namespace ioj::levels {
struct XorExpr {
    auto operator==(XorExpr const&) const -> bool = default;
    static constexpr auto type() -> ExprKind { return ExprKind::Xor; }
    std::vector<ExprIndex> children;
};
}
