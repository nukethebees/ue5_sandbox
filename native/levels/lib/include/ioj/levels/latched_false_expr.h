#pragma once
#include <ioj/levels/expr_type.h>

#include <cstdint>
namespace ioj::levels {
struct LatchedFalseExpr {
    auto operator==(LatchedFalseExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::LatchedFalse};
    std::uint32_t latch{};
};
}
