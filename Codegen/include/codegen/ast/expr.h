#pragma once

#include <codegen/ast/cpp_type.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace codegen {

struct NamedExpr;
struct LiteralExpr;
struct CallExpr;
struct MemberAccessExpr;
struct SubscriptExpr;
struct BinaryExpr;
struct StaticCastExpr;
struct InitializerListExpr;
struct RawExpr;
struct ExprData;

class Expr {
  public:
    Expr(NamedExpr value);
    Expr(LiteralExpr value);
    Expr(CallExpr value);
    Expr(MemberAccessExpr value);
    Expr(SubscriptExpr value);
    Expr(BinaryExpr value);
    Expr(StaticCastExpr value);
    Expr(InitializerListExpr value);
    Expr(RawExpr value);

    auto value() const -> ExprData const&;
  private:
    std::shared_ptr<ExprData const> value_;
};

struct NamedExpr {
    std::string spelling;
    std::vector<TypeDependency> dependencies;
};

struct LiteralExpr {
    // A complete literal token; use string_literal() for unescaped string contents.
    std::string spelling;
};

struct CallExpr {
    Expr callee;
    std::vector<Expr> arguments;
};

struct MemberAccessExpr {
    Expr object;
    std::string member;
};

struct SubscriptExpr {
    Expr object;
    Expr index;
};

enum class BinaryOperator {
    equal,
    greater_equal,
    subtract,
};

struct BinaryExpr {
    BinaryOperator operation;
    Expr left;
    Expr right;
};

struct StaticCastExpr {
    CppType type;
    Expr operand;
};

struct InitializerListExpr {
    std::optional<CppType> type;
    std::vector<Expr> elements;
};

struct RawExpr {
    std::string text;
    std::vector<TypeDependency> dependencies;
};

using ExprValue = std::variant<NamedExpr,
                               LiteralExpr,
                               CallExpr,
                               MemberAccessExpr,
                               SubscriptExpr,
                               BinaryExpr,
                               StaticCastExpr,
                               InitializerListExpr,
                               RawExpr>;

struct ExprData : ExprValue {
    using ExprValue::ExprValue;
};

auto named(std::string spelling, std::vector<TypeDependency> dependencies = {}) -> Expr;
auto literal(std::string spelling) -> Expr;
auto string_literal(std::string_view value) -> Expr;
auto call(Expr callee, std::vector<Expr> arguments = {}) -> Expr;
auto member_access(Expr object, std::string member) -> Expr;
auto subscript(Expr object, Expr index) -> Expr;
auto binary(BinaryOperator operation, Expr left, Expr right) -> Expr;
auto static_cast_expr(CppType type, Expr operand) -> Expr;
auto init_list(std::vector<Expr> elements, std::optional<CppType> type = std::nullopt) -> Expr;

auto render(Expr const& expression) -> std::string;
auto dependencies(Expr const& expression) -> std::vector<TypeDependency>;

} // namespace codegen
