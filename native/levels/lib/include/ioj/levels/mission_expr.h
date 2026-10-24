#pragma once

#include <ioj/levels/expr_payload.h>

#include <span>
#include <utility>

namespace ioj::levels {
struct MissionExpr {
    MissionExpr() = default;
    MissionExpr(ExprPayload value)
        : payload{std::move(value)} {}

    auto operator==(MissionExpr const&) const -> bool = default;

    auto type() const -> ExprType;
    auto children() const -> std::span<ExprIndex const>;

    ExprPayload payload{AllExpr{}};
};
}
