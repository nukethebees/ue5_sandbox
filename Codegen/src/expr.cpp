#include <codegen/ast/expr.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen {
namespace {

enum class ExprPrecedence {
    raw,
    equality,
    relational,
    additive,
    postfix,
};

auto binary_precedence(BinaryOperator const operation) -> ExprPrecedence {
    switch (operation) {
        case BinaryOperator::equal:
            return ExprPrecedence::equality;
        case BinaryOperator::greater_equal:
            return ExprPrecedence::relational;
        case BinaryOperator::subtract:
            return ExprPrecedence::additive;
    }
    throw std::invalid_argument{"Unsupported binary operator"};
}

auto precedence(Expr const& expression) -> ExprPrecedence {
    return std::visit(
        [](auto const& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, RawExpr>) {
                return ExprPrecedence::raw;
            } else if constexpr (std::is_same_v<T, BinaryExpr>) {
                return binary_precedence(value.operation);
            } else {
                return ExprPrecedence::postfix;
            }
        },
        expression.value());
}

auto render_operand(Expr const& expression,
                    ExprPrecedence const minimum_precedence,
                    bool const parenthesize_equal = false) -> std::string {
    auto text{render(expression)};
    auto const operand_precedence{precedence(expression)};
    return operand_precedence < minimum_precedence ||
                   (parenthesize_equal && operand_precedence == minimum_precedence)
             ? "(" + text + ")"
             : text;
}

auto render_arguments(std::vector<Expr> const& arguments) -> std::string {
    std::string result;
    auto const count{arguments.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += render_operand(arguments[index], ExprPrecedence::equality);
    }
    return result;
}

auto binary_spelling(BinaryOperator const operation) -> std::string_view {
    switch (operation) {
        case BinaryOperator::equal:
            return " == ";
        case BinaryOperator::greater_equal:
            return " >= ";
        case BinaryOperator::subtract:
            return " - ";
    }
    throw std::invalid_argument{"Unsupported binary operator"};
}

} // namespace

Expr::Expr(NamedExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(LiteralExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(CallExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(MemberAccessExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(SubscriptExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(BinaryExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(StaticCastExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(InitializerListExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(RawExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
auto Expr::value() const -> ExprData const& {
    return *value_;
}

auto named(std::string spelling, std::vector<TypeDependency> dependencies) -> Expr {
    return NamedExpr{std::move(spelling), std::move(dependencies)};
}
auto literal(std::string spelling) -> Expr {
    return LiteralExpr{std::move(spelling)};
}
auto string_literal(std::string_view const value) -> Expr {
    std::string result{"\""};
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default: {
                auto const byte{static_cast<unsigned char>(character)};
                if (byte < 32 || byte == 127) {
                    // Three octal digits cannot consume a following digit in the string.
                    result += '\\';
                    result += static_cast<char>('0' + ((byte >> 6) & 7));
                    result += static_cast<char>('0' + ((byte >> 3) & 7));
                    result += static_cast<char>('0' + (byte & 7));
                } else {
                    result += character;
                }
                break;
            }
        }
    }
    result += '"';
    return literal(std::move(result));
}
auto call(Expr callee, std::vector<Expr> arguments) -> Expr {
    return CallExpr{std::move(callee), std::move(arguments)};
}
auto member_access(Expr object, std::string member) -> Expr {
    return MemberAccessExpr{std::move(object), std::move(member)};
}
auto subscript(Expr object, Expr index) -> Expr {
    return SubscriptExpr{std::move(object), std::move(index)};
}
auto binary(BinaryOperator const operation, Expr left, Expr right) -> Expr {
    return BinaryExpr{operation, std::move(left), std::move(right)};
}
auto static_cast_expr(CppType type, Expr operand) -> Expr {
    return StaticCastExpr{std::move(type), std::move(operand)};
}
auto init_list(std::vector<Expr> elements, std::optional<CppType> type) -> Expr {
    return InitializerListExpr{std::move(type), std::move(elements)};
}

auto render(Expr const& expression) -> std::string {
    return std::visit(
        [](auto const& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, NamedExpr> || std::is_same_v<T, LiteralExpr>) {
                return value.spelling;
            } else if constexpr (std::is_same_v<T, RawExpr>) {
                return value.text;
            } else if constexpr (std::is_same_v<T, CallExpr>) {
                return render_operand(value.callee, ExprPrecedence::postfix) + "(" +
                       render_arguments(value.arguments) + ")";
            } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
                return render_operand(value.object, ExprPrecedence::postfix) + "." + value.member;
            } else if constexpr (std::is_same_v<T, SubscriptExpr>) {
                return render_operand(value.object, ExprPrecedence::postfix) + "[" +
                       render_operand(value.index, ExprPrecedence::equality) + "]";
            } else if constexpr (std::is_same_v<T, BinaryExpr>) {
                auto const parent_precedence{binary_precedence(value.operation)};
                return render_operand(value.left, parent_precedence) +
                       std::string{binary_spelling(value.operation)} +
                       render_operand(value.right, parent_precedence, true);
            } else if constexpr (std::is_same_v<T, StaticCastExpr>) {
                return "static_cast<" + value.type.spelling + ">(" +
                       render_operand(value.operand, ExprPrecedence::equality) + ")";
            } else if constexpr (std::is_same_v<T, InitializerListExpr>) {
                return (value.type.has_value() ? value.type->spelling : "") + "{" +
                       render_arguments(value.elements) + "}";
            }
        },
        expression.value());
}

auto dependencies(Expr const& expression) -> std::vector<TypeDependency> {
    return std::visit(
        [](auto const& value) -> std::vector<TypeDependency> {
            using T = std::decay_t<decltype(value)>;
            std::vector<TypeDependency> result;
            auto append = [&](Expr const& child) {
                auto const nested{dependencies(child)};
                result.insert(result.end(), nested.begin(), nested.end());
            };

            if constexpr (std::is_same_v<T, NamedExpr> || std::is_same_v<T, RawExpr>) {
                result = value.dependencies;
            } else if constexpr (std::is_same_v<T, CallExpr>) {
                append(value.callee);
                for (auto const& argument : value.arguments) {
                    append(argument);
                }
            } else if constexpr (std::is_same_v<T, BinaryExpr>) {
                append(value.left);
                append(value.right);
            } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
                append(value.object);
            } else if constexpr (std::is_same_v<T, SubscriptExpr>) {
                append(value.object);
                append(value.index);
            } else if constexpr (std::is_same_v<T, StaticCastExpr>) {
                result = value.type.dependencies;
                append(value.operand);
            } else if constexpr (std::is_same_v<T, InitializerListExpr>) {
                if (value.type.has_value()) {
                    result = value.type->dependencies;
                }
                for (auto const& element : value.elements) {
                    append(element);
                }
            }
            return result;
        },
        expression.value());
}

} // namespace codegen
