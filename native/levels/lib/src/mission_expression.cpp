#include <ioj/levels/mission_expression.h>

namespace ioj::levels {
auto MissionExpression::type() const -> ExpressionType {
    return std::visit([](auto const& value) { return value.type; }, payload);
}
auto MissionExpression::children() const -> std::span<ExpressionIndex const> {
    if (auto const* all{std::get_if<AllExpression>(&payload)}) {
        return all->children;
    }
    if (auto const* any{std::get_if<AnyExpression>(&payload)}) {
        return any->children;
    }
    return {};
}
}
