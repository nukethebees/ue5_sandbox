#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/cpp_string.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

namespace expr_detail {
auto handle(ExprKind kind, std::size_t index) -> Expr {
    assert(index < 0x01000000U);
    return Expr{(static_cast<std::uint32_t>(kind) << 24) | static_cast<std::uint32_t>(index)};
}
auto binary_precedence(BinaryOperator operation) -> int {
    switch (operation) {
        case BinaryOperator::logical_or:
            return 1;
        case BinaryOperator::logical_and:
            return 2;
        case BinaryOperator::equal:
        case BinaryOperator::not_equal:
            return 3;
        case BinaryOperator::greater_equal:
        case BinaryOperator::greater:
        case BinaryOperator::less:
            return 4;
        case BinaryOperator::subtract:
        case BinaryOperator::add:
            return 5;
        case BinaryOperator::multiply:
        case BinaryOperator::divide:
            return 6;
    }
    std::unreachable();
}
auto binary_spelling(BinaryOperator operation) -> std::string_view {
    switch (operation) {
        case BinaryOperator::logical_or:
            return " || ";
        case BinaryOperator::logical_and:
            return " && ";
        case BinaryOperator::equal:
            return " == ";
        case BinaryOperator::not_equal:
            return " != ";
        case BinaryOperator::greater_equal:
            return " >= ";
        case BinaryOperator::greater:
            return " > ";
        case BinaryOperator::less:
            return " < ";
        case BinaryOperator::subtract:
            return " - ";
        case BinaryOperator::add:
            return " + ";
        case BinaryOperator::multiply:
            return " * ";
        case BinaryOperator::divide:
            return " / ";
    }
    std::unreachable();
}
} // namespace expr_detail
auto CppIR::named(std::string_view value, std::span<TypeDependencyId const> dependencies) -> Expr {
    return named(strings.intern(value), dependencies);
}
auto CppIR::named(IdentifierId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{expr_detail::handle(ExprKind::named, named_exprs_.spelling.size())};
    named_exprs_.spelling.push_back(string_id(value));
    named_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::literal(std::string_view value) -> Expr {
    return literal(strings.add(value));
}
auto CppIR::literal(StringId value) -> Expr {
    auto const result{expr_detail::handle(ExprKind::literal, literal_exprs_.spelling.size())};
    literal_exprs_.spelling.push_back(value);
    return result;
}
auto CppIR::string_literal(std::string_view value) -> Expr {
    return string_literal(strings.add(value));
}
auto CppIR::string_literal(StringId value) -> Expr {
    auto const result{expr_detail::handle(ExprKind::string_literal, string_literal_exprs_.size())};
    string_literal_exprs_.push_back(value);
    return result;
}
auto CppIR::raw_expr(std::string_view value, std::span<TypeDependencyId const> dependencies)
    -> Expr {
    return raw_expr(strings.add(value), dependencies);
}
auto CppIR::raw_expr(StringId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{expr_detail::handle(ExprKind::raw, raw_exprs_.text.size())};
    raw_exprs_.text.push_back(value);
    raw_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::call(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{expr_detail::handle(ExprKind::call, call_exprs_.callee.size())};
    call_exprs_.callee.push_back(callee);
    call_exprs_.arguments.push_back(save_exprs(arguments));
    return result;
}
auto CppIR::member_access(Expr object, std::string_view member) -> Expr {
    return member_access(object, strings.intern(member));
}
auto CppIR::member_access(Expr object, IdentifierId member) -> Expr {
    auto const result{expr_detail::handle(ExprKind::member_access, member_exprs_.object.size())};
    member_exprs_.object.push_back(object);
    member_exprs_.member.push_back(string_id(member));
    member_exprs_.through_pointer.push_back(false);
    return result;
}
auto CppIR::pointer_member_access(Expr object, std::string_view member) -> Expr {
    return pointer_member_access(object, strings.intern(member));
}
auto CppIR::pointer_member_access(Expr object, IdentifierId member) -> Expr {
    auto const result{member_access(object, member)};
    member_exprs_.through_pointer[result.index()] = true;
    return result;
}
auto CppIR::unary(UnaryOperator operation, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::unary, unary_exprs_.operation.size())};
    unary_exprs_.operation.push_back(operation);
    unary_exprs_.operand.push_back(operand);
    return result;
}
auto CppIR::subscript(Expr object, Expr index) -> Expr {
    auto const result{expr_detail::handle(ExprKind::subscript, subscript_exprs_.object.size())};
    subscript_exprs_.object.push_back(object);
    subscript_exprs_.index.push_back(index);
    return result;
}
auto CppIR::binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after) -> Expr {
    auto const result{expr_detail::handle(ExprKind::binary, binary_exprs_.operation.size())};
    binary_exprs_.operation.push_back(operation);
    binary_exprs_.left.push_back(left);
    binary_exprs_.right.push_back(right);
    binary_exprs_.line_break_after.push_back(line_break_after);
    return result;
}
auto CppIR::static_cast_expr(CppTypeId type, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::static_cast_, cast_exprs_.type.size())};
    cast_exprs_.type.push_back(type);
    cast_exprs_.operand.push_back(operand);
    return result;
}
auto CppIR::sizeof_type(CppTypeId type) -> Expr {
    auto const result{expr_detail::handle(ExprKind::sizeof_type, sizeof_exprs_.type.size())};
    sizeof_exprs_.type.push_back(type);
    return result;
}
auto CppIR::init_list(std::span<Expr const> elements, std::optional<CppTypeId> type) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::initializer_list, initializer_exprs_.type.size())};
    initializer_exprs_.type.push_back(type);
    initializer_exprs_.elements.push_back(save_exprs(elements));
    return result;
}
auto CppIR::type_reference(CppTypeId type) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::type_reference, type_reference_exprs_.type.size())};
    type_reference_exprs_.type.push_back(type);
    return result;
}
auto CppIR::parenthesized(Expr expr) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::parenthesized, parenthesized_exprs_.operand.size())};
    parenthesized_exprs_.operand.push_back(expr);
    return result;
}
auto CppIR::template_id(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{expr_detail::handle(ExprKind::template_id, template_id_exprs_.callee.size())};
    template_id_exprs_.callee.push_back(callee);
    template_id_exprs_.arguments.push_back(save_exprs(arguments));
    return result;
}

auto CppIR::expr_precedence(Expr expr) const -> int {
    switch (expr.kind()) {
        case ExprKind::raw:
            return 0;
        case ExprKind::binary:
            return expr_detail::binary_precedence(binary_exprs_.operation[expr.index()]);
        case ExprKind::sizeof_type:
        case ExprKind::unary:
            return 7;
        default:
            return 8;
    }
}
void CppIR::append_expr(
    std::string& output, Expr expr, RenderContext const& context, int minimum, bool equal) const {
    auto const index{expr.index()};
    auto const precedence{expr_precedence(expr)};
    auto const parenthesize{precedence < minimum || (equal && precedence == minimum)};
    if (parenthesize) {
        output += '(';
    }
    auto append_arguments = [&](ir::Range range) {
        bool first{true};
        for (auto const argument : exprs(range)) {
            if (!first) {
                output += ", ";
            }
            append_expr(output, argument, context, 1);
            first = false;
        }
    };
    switch (expr.kind()) {
        case ExprKind::type_reference:
            types.append(output, type_reference_exprs_.type[index], context);
            break;
        case ExprKind::parenthesized:
            output += '(';
            append_expr(output, parenthesized_exprs_.operand[index], context);
            output += ')';
            break;
        case ExprKind::template_id:
            append_expr(output, template_id_exprs_.callee[index], context, 8);
            output += '<';
            append_arguments(template_id_exprs_.arguments[index]);
            output += '>';
            break;
        case ExprKind::named:
            output += strings.view(named_exprs_.spelling[index]);
            break;
        case ExprKind::literal:
            output += strings.view(literal_exprs_.spelling[index]);
            break;
        case ExprKind::string_literal:
            output += '"';
            append_escaped_cpp_string(output, strings.view(string_literal_exprs_[index]));
            output += '"';
            break;
        case ExprKind::raw:
            output += strings.view(raw_exprs_.text[index]);
            break;
        case ExprKind::call:
            append_expr(output, call_exprs_.callee[index], context, 8);
            output += '(';
            append_arguments(call_exprs_.arguments[index]);
            output += ')';
            break;
        case ExprKind::member_access:
            append_expr(output, member_exprs_.object[index], context, 8);
            output += member_exprs_.through_pointer[index] ? "->" : ".";
            output += strings.view(member_exprs_.member[index]);
            break;
        case ExprKind::subscript:
            append_expr(output, subscript_exprs_.object[index], context, 8);
            output += '[';
            append_expr(output, subscript_exprs_.index[index], context, 1);
            output += ']';
            break;
        case ExprKind::binary:
            append_expr(output, binary_exprs_.left[index], context, precedence);
            output += expr_detail::binary_spelling(binary_exprs_.operation[index]);
            if (binary_exprs_.line_break_after[index]) {
                output.pop_back();
                output += "\n    ";
            }
            append_expr(output, binary_exprs_.right[index], context, precedence, true);
            break;
        case ExprKind::unary:
            switch (unary_exprs_.operation[index]) {
                case UnaryOperator::logical_not:
                    output += '!';
                    break;
                case UnaryOperator::dereference:
                    output += '*';
                    break;
                case UnaryOperator::address_of:
                    output += '&';
                    break;
                case UnaryOperator::preincrement:
                    output += "++";
                    break;
            }
            append_expr(output, unary_exprs_.operand[index], context, 7, true);
            break;
        case ExprKind::static_cast_:
            output += "static_cast<";
            types.append(output, cast_exprs_.type[index], context);
            output += ">(";
            append_expr(output, cast_exprs_.operand[index], context, 1);
            output += ')';
            break;
        case ExprKind::sizeof_type:
            output += "sizeof(";
            types.append(output, sizeof_exprs_.type[index], context);
            output += ')';
            break;
        case ExprKind::initializer_list:
            if (auto const type{initializer_exprs_.type[index]}) {
                types.append(output, *type, context);
            }
            output += '{';
            append_arguments(initializer_exprs_.elements[index]);
            output += '}';
            break;
        case ExprKind::invalid:
            std::unreachable();
    }
    if (parenthesize) {
        output += ')';
    }
}
auto CppIR::render(Expr expr, RenderContext const& context) const -> std::string {
    RenderState state;
    auto active{context};
    if (active.state == nullptr) {
        active.state = &state;
    }
    std::string result;
    append_expr(result, expr, active);
    return result;
}
void CppIR::collect_dependencies(Expr expr, std::vector<TypeDependencyId>& result) const {
    auto const index{expr.index()};
    auto append = [&](std::span<TypeDependencyId const> values) {
        result.insert(result.end(), values.begin(), values.end());
    };
    switch (expr.kind()) {
        case ExprKind::type_reference:
            append(types.dependencies(type_reference_exprs_.type[index]));
            break;
        case ExprKind::parenthesized:
            collect_dependencies(parenthesized_exprs_.operand[index], result);
            break;
        case ExprKind::template_id:
            collect_dependencies(template_id_exprs_.callee[index], result);
            for (auto const argument : exprs(template_id_exprs_.arguments[index])) {
                collect_dependencies(argument, result);
            }
            break;
        case ExprKind::named:
            append(dependency_list(named_exprs_.dependencies[index]));
            break;
        case ExprKind::raw:
            append(dependency_list(raw_exprs_.dependencies[index]));
            break;
        case ExprKind::call:
            collect_dependencies(call_exprs_.callee[index], result);
            for (auto const child : exprs(call_exprs_.arguments[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::member_access:
            collect_dependencies(member_exprs_.object[index], result);
            break;
        case ExprKind::subscript:
            collect_dependencies(subscript_exprs_.object[index], result);
            collect_dependencies(subscript_exprs_.index[index], result);
            break;
        case ExprKind::binary:
            collect_dependencies(binary_exprs_.left[index], result);
            collect_dependencies(binary_exprs_.right[index], result);
            break;
        case ExprKind::unary:
            collect_dependencies(unary_exprs_.operand[index], result);
            break;
        case ExprKind::static_cast_:
            append(types.dependencies(cast_exprs_.type[index]));
            collect_dependencies(cast_exprs_.operand[index], result);
            break;
        case ExprKind::sizeof_type:
            append(types.dependencies(sizeof_exprs_.type[index]));
            break;
        case ExprKind::initializer_list:
            if (auto const type{initializer_exprs_.type[index]}) {
                append(types.dependencies(*type));
            }
            for (auto const child : exprs(initializer_exprs_.elements[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::literal:
        case ExprKind::string_literal:
            break;
        case ExprKind::invalid:
            std::unreachable();
    }
}

} // namespace lispb::codegen
