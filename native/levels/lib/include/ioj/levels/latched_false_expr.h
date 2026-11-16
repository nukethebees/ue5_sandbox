#pragma once
#include <ioj/expr_kind.h>

#include <cstdint>
namespace ioj::levels {
struct LatchedFalseExpr {
    auto operator==(LatchedFalseExpr const&) const -> bool = default;
    static constexpr ExprKind type{ExprKind::LatchedFalse};
    std::uint32_t latch{};
};
}
