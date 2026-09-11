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
struct UnaryExpr;
struct StaticCastExpr;
struct SizeofTypeExpr;
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
    Expr(UnaryExpr value);
    Expr(StaticCastExpr value);
    Expr(SizeofTypeExpr value);
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
    bool through_pointer{false};
};

struct SubscriptExpr {
    Expr object;
    Expr index;
};

enum class BinaryOperator {
    equal,
    greater_equal,
    subtract,
    add,
    multiply,
    logical_or,
    not_equal,
    divide,
};

enum class UnaryOperator { logical_not, dereference, address_of };

struct UnaryExpr {
    UnaryOperator operation;
    Expr operand;
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

struct SizeofTypeExpr {
    CppType type;
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
                               UnaryExpr,
                               StaticCastExpr,
                               SizeofTypeExpr,
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
auto pointer_member_access(Expr object, std::string member) -> Expr;
auto unary(UnaryOperator operation, Expr operand) -> Expr;
auto subscript(Expr object, Expr index) -> Expr;
auto binary(BinaryOperator operation, Expr left, Expr right) -> Expr;
auto static_cast_expr(CppType type, Expr operand) -> Expr;
auto sizeof_type(CppType type) -> Expr;
auto init_list(std::vector<Expr> elements, std::optional<CppType> type = std::nullopt) -> Expr;

auto render(Expr const& expression) -> std::string;
auto dependencies(Expr const& expression) -> std::vector<TypeDependency>;

} // namespace codegen
