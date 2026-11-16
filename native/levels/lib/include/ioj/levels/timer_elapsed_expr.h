#pragma once
#include <ioj/expr_kind.h>

#include <cstdint>
namespace ioj::levels {
struct TimerElapsedExpr {
    auto operator==(TimerElapsedExpr const&) const -> bool = default;
    static constexpr ExprKind type{ExprKind::TimerElapsed};
    std::uint32_t timer{};
};
}
