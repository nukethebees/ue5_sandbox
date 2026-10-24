#include <ioj/levels/mission_expr.h>

namespace ioj::levels {
auto MissionExpr::type() const -> ExprType {
    return std::visit([](auto const& value) { return value.type; }, payload);
}
auto MissionExpr::children() const -> std::span<ExprIndex const> {
    if (auto const* all{std::get_if<AllExpr>(&payload)}) {
        return all->children;
    }
    if (auto const* any{std::get_if<AnyExpr>(&payload)}) {
        return any->children;
    }
    return {};
}
}
