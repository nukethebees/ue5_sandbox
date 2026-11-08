#include <ioj/levels/expression.h>
namespace ioj::levels {
auto Expression::type() const -> ExprType {
    return std::visit([](auto const& value) { return value.type; }, payload);
}
auto Expression::children() const -> std::span<ExprIndex const> {
    return std::visit(
        [](auto const& value) -> std::span<ExprIndex const> {
            if constexpr (requires { value.children; }) {
                return value.children;
            } else {
                return {};
            }
        },
        payload);
}
}
