#pragma once

#include <ioj/levels/expression_payload.h>

#include <span>
#include <utility>

namespace ioj::levels {
// Keep the tag in the variant so a condition cannot carry another condition's data.
struct MissionExpression {
    ExpressionPayload payload{AllExpression{}};
    MissionExpression() = default;
    MissionExpression(ExpressionPayload value)
        : payload{std::move(value)} {}
    auto type() const -> ExpressionType;
    auto children() const -> std::span<ExpressionIndex const>;
    auto operator==(MissionExpression const&) const -> bool = default;
};
}
