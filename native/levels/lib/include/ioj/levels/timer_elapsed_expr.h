#pragma once
#include <ioj/levels/expr_type.h>

#include <cstdint>
namespace ioj::levels {
struct TimerElapsedExpr {
    auto operator==(TimerElapsedExpr const&) const -> bool = default;
    static constexpr ExprType type{ExprType::TimerElapsed};
    std::uint32_t timer{};
};
}
