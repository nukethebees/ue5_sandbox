#include <codegen/ast/expr.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace codegen {
namespace {

enum class ExprPrecedence {
    raw,
    logical_or,
    equality,
    relational,
    additive,
    multiplicative,
    unary,
    postfix,
};

auto binary_precedence(BinaryOperator const operation) -> ExprPrecedence {
    switch (operation) {
        case BinaryOperator::logical_or:
            return ExprPrecedence::logical_or;
        case BinaryOperator::equal:
        case BinaryOperator::not_equal:
            return ExprPrecedence::equality;
        case BinaryOperator::greater_equal:
            return ExprPrecedence::relational;
        case BinaryOperator::subtract:
        case BinaryOperator::add:
            return ExprPrecedence::additive;
        case BinaryOperator::multiply:
        case BinaryOperator::divide:
            return ExprPrecedence::multiplicative;
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
            } else if constexpr (std::is_same_v<T, SizeofTypeExpr> ||
                                 std::is_same_v<T, UnaryExpr>) {
                return ExprPrecedence::unary;
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
        result += render_operand(arguments[index], ExprPrecedence::logical_or);
    }
    return result;
}

auto binary_spelling(BinaryOperator const operation) -> std::string_view {
    switch (operation) {
        case BinaryOperator::logical_or:
            return " || ";
        case BinaryOperator::equal:
            return " == ";
        case BinaryOperator::not_equal:
            return " != ";
        case BinaryOperator::divide:
            return " / ";
        case BinaryOperator::greater_equal:
            return " >= ";
        case BinaryOperator::subtract:
            return " - ";
        case BinaryOperator::add:
            return " + ";
        case BinaryOperator::multiply:
            return " * ";
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
Expr::Expr(UnaryExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(StaticCastExpr value)
    : value_{std::make_shared<ExprData const>(std::move(value))} {}
Expr::Expr(SizeofTypeExpr value)
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
auto pointer_member_access(Expr object, std::string member) -> Expr {
    return MemberAccessExpr{std::move(object), std::move(member), true};
}
auto unary(UnaryOperator operation, Expr operand) -> Expr {
    return UnaryExpr{operation, std::move(operand)};
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
auto sizeof_type(CppType type) -> Expr {
    return SizeofTypeExpr{std::move(type)};
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
                return render_operand(value.object, ExprPrecedence::postfix) +
                       (value.through_pointer ? "->" : ".") + value.member;
            } else if constexpr (std::is_same_v<T, UnaryExpr>) {
                std::string token;
                switch (value.operation) {
                    case UnaryOperator::logical_not:
                        token = "!";
                        break;
                    case UnaryOperator::dereference:
                        token = "*";
                        break;
                    case UnaryOperator::address_of:
                        token = "&";
                        break;
                }
                return token + render_operand(value.operand, ExprPrecedence::unary, true);
            } else if constexpr (std::is_same_v<T, SubscriptExpr>) {
                return render_operand(value.object, ExprPrecedence::postfix) + "[" +
                       render_operand(value.index, ExprPrecedence::logical_or) + "]";
            } else if constexpr (std::is_same_v<T, BinaryExpr>) {
                auto const parent_precedence{binary_precedence(value.operation)};
                return render_operand(value.left, parent_precedence) +
                       std::string{binary_spelling(value.operation)} +
                       render_operand(value.right, parent_precedence, true);
            } else if constexpr (std::is_same_v<T, StaticCastExpr>) {
                return "static_cast<" + value.type.spelling + ">(" +
                       render_operand(value.operand, ExprPrecedence::logical_or) + ")";
            } else if constexpr (std::is_same_v<T, SizeofTypeExpr>) {
                return "sizeof(" + value.type.spelling + ")";
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
            } else if constexpr (std::is_same_v<T, UnaryExpr>) {
                append(value.operand);
            } else if constexpr (std::is_same_v<T, SubscriptExpr>) {
                append(value.object);
                append(value.index);
            } else if constexpr (std::is_same_v<T, StaticCastExpr>) {
                result = value.type.dependencies;
                append(value.operand);
            } else if constexpr (std::is_same_v<T, SizeofTypeExpr>) {
                result = value.type.dependencies;
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
