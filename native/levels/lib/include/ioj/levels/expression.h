#pragma once

#include <ioj/levels/expr_payload.h>

#include <span>
#include <utility>

namespace ioj::levels {
struct Expression {
    Expression() = default;
    Expression(ExprPayload value)
        : payload{std::move(value)} {}

    auto operator==(Expression const&) const -> bool = default;

    auto type() const -> ExprType;
    auto children() const -> std::span<ExprIndex const>;

    ExprPayload payload{AllExpr{}};
};
}
