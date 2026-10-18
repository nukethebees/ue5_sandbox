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

namespace expression_detail {
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
} // namespace expression_detail
auto CppIR::named(std::string_view value, std::span<TypeDependencyId const> dependencies) -> Expr {
    return named(strings.intern(value), dependencies);
}
auto CppIR::named(IdentifierId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::named, named_expressions_.spelling.size())};
    named_expressions_.spelling.push_back(string_id(value));
    named_expressions_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::literal(std::string_view value) -> Expr {
    return literal(strings.add(value));
}
auto CppIR::literal(StringId value) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::literal, literal_expressions_.spelling.size())};
    literal_expressions_.spelling.push_back(value);
    return result;
}
auto CppIR::string_literal(std::string_view value) -> Expr {
    return string_literal(strings.add(value));
}
auto CppIR::string_literal(StringId value) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::string_literal, string_literal_expressions_.size())};
    string_literal_expressions_.push_back(value);
    return result;
}
auto CppIR::raw_expression(std::string_view value, std::span<TypeDependencyId const> dependencies)
    -> Expr {
    return raw_expression(strings.add(value), dependencies);
}
auto CppIR::raw_expression(StringId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{expression_detail::handle(ExprKind::raw, raw_expressions_.text.size())};
    raw_expressions_.text.push_back(value);
    raw_expressions_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::call(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{expression_detail::handle(ExprKind::call, call_expressions_.callee.size())};
    call_expressions_.callee.push_back(callee);
    call_expressions_.arguments.push_back(save_expressions(arguments));
    return result;
}
auto CppIR::member_access(Expr object, std::string_view member) -> Expr {
    return member_access(object, strings.intern(member));
}
auto CppIR::member_access(Expr object, IdentifierId member) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::member_access, member_expressions_.object.size())};
    member_expressions_.object.push_back(object);
    member_expressions_.member.push_back(string_id(member));
    member_expressions_.through_pointer.push_back(false);
    return result;
}
auto CppIR::pointer_member_access(Expr object, std::string_view member) -> Expr {
    return pointer_member_access(object, strings.intern(member));
}
auto CppIR::pointer_member_access(Expr object, IdentifierId member) -> Expr {
    auto const result{member_access(object, member)};
    member_expressions_.through_pointer[result.index()] = true;
    return result;
}
auto CppIR::unary(UnaryOperator operation, Expr operand) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::unary, unary_expressions_.operation.size())};
    unary_expressions_.operation.push_back(operation);
    unary_expressions_.operand.push_back(operand);
    return result;
}
auto CppIR::subscript(Expr object, Expr index) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::subscript, subscript_expressions_.object.size())};
    subscript_expressions_.object.push_back(object);
    subscript_expressions_.index.push_back(index);
    return result;
}
auto CppIR::binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::binary, binary_expressions_.operation.size())};
    binary_expressions_.operation.push_back(operation);
    binary_expressions_.left.push_back(left);
    binary_expressions_.right.push_back(right);
    binary_expressions_.line_break_after.push_back(line_break_after);
    return result;
}
auto CppIR::static_cast_expr(CppTypeId type, Expr operand) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::static_cast_, cast_expressions_.type.size())};
    cast_expressions_.type.push_back(type);
    cast_expressions_.operand.push_back(operand);
    return result;
}
auto CppIR::sizeof_type(CppTypeId type) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::sizeof_type, sizeof_expressions_.type.size())};
    sizeof_expressions_.type.push_back(type);
    return result;
}
auto CppIR::init_list(std::span<Expr const> elements, std::optional<CppTypeId> type) -> Expr {
    auto const result{expression_detail::handle(ExprKind::initializer_list,
                                                initializer_expressions_.type.size())};
    initializer_expressions_.type.push_back(type);
    initializer_expressions_.elements.push_back(save_expressions(elements));
    return result;
}
auto CppIR::type_reference(CppTypeId type) -> Expr {
    auto const result{expression_detail::handle(ExprKind::type_reference,
                                                type_reference_expressions_.type.size())};
    type_reference_expressions_.type.push_back(type);
    return result;
}
auto CppIR::parenthesized(Expr expression) -> Expr {
    auto const result{expression_detail::handle(ExprKind::parenthesized,
                                                parenthesized_expressions_.operand.size())};
    parenthesized_expressions_.operand.push_back(expression);
    return result;
}
auto CppIR::template_id(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{
        expression_detail::handle(ExprKind::template_id, template_id_expressions_.callee.size())};
    template_id_expressions_.callee.push_back(callee);
    template_id_expressions_.arguments.push_back(save_expressions(arguments));
    return result;
}

auto CppIR::expression_precedence(Expr expression) const -> int {
    switch (expression.kind()) {
        case ExprKind::raw:
            return 0;
        case ExprKind::binary:
            return expression_detail::binary_precedence(
                binary_expressions_.operation[expression.index()]);
        case ExprKind::sizeof_type:
        case ExprKind::unary:
            return 7;
        default:
            return 8;
    }
}
void CppIR::append_expression(std::string& output, Expr expression, int minimum, bool equal) const {
    auto const index{expression.index()};
    auto const precedence{expression_precedence(expression)};
    auto const parenthesize{precedence < minimum || (equal && precedence == minimum)};
    if (parenthesize) {
        output += '(';
    }
    auto append_arguments = [&](ir::Range range) {
        bool first{true};
        for (auto const argument : expressions(range)) {
            if (!first) {
                output += ", ";
            }
            append_expression(output, argument, 1);
            first = false;
        }
    };
    switch (expression.kind()) {
        case ExprKind::type_reference:
            output += types.spelling(type_reference_expressions_.type[index]);
            break;
        case ExprKind::parenthesized:
            output += '(';
            append_expression(output, parenthesized_expressions_.operand[index]);
            output += ')';
            break;
        case ExprKind::template_id:
            append_expression(output, template_id_expressions_.callee[index], 8);
            output += '<';
            append_arguments(template_id_expressions_.arguments[index]);
            output += '>';
            break;
        case ExprKind::named:
            output += strings.view(named_expressions_.spelling[index]);
            break;
        case ExprKind::literal:
            output += strings.view(literal_expressions_.spelling[index]);
            break;
        case ExprKind::string_literal:
            output += '"';
            append_escaped_cpp_string(output, strings.view(string_literal_expressions_[index]));
            output += '"';
            break;
        case ExprKind::raw:
            output += strings.view(raw_expressions_.text[index]);
            break;
        case ExprKind::call:
            append_expression(output, call_expressions_.callee[index], 8);
            output += '(';
            append_arguments(call_expressions_.arguments[index]);
            output += ')';
            break;
        case ExprKind::member_access:
            append_expression(output, member_expressions_.object[index], 8);
            output += member_expressions_.through_pointer[index] ? "->" : ".";
            output += strings.view(member_expressions_.member[index]);
            break;
        case ExprKind::subscript:
            append_expression(output, subscript_expressions_.object[index], 8);
            output += '[';
            append_expression(output, subscript_expressions_.index[index], 1);
            output += ']';
            break;
        case ExprKind::binary:
            append_expression(output, binary_expressions_.left[index], precedence);
            output += expression_detail::binary_spelling(binary_expressions_.operation[index]);
            if (binary_expressions_.line_break_after[index]) {
                output.pop_back();
                output += "\n    ";
            }
            append_expression(output, binary_expressions_.right[index], precedence, true);
            break;
        case ExprKind::unary:
            switch (unary_expressions_.operation[index]) {
                case UnaryOperator::logical_not:
                    output += '!';
                    break;
                case UnaryOperator::dereference:
                    output += '*';
                    break;
                case UnaryOperator::address_of:
                    output += '&';
                    break;
            }
            append_expression(output, unary_expressions_.operand[index], 7, true);
            break;
        case ExprKind::static_cast_:
            output += "static_cast<";
            output += types.spelling(cast_expressions_.type[index]);
            output += ">(";
            append_expression(output, cast_expressions_.operand[index], 1);
            output += ')';
            break;
        case ExprKind::sizeof_type:
            output += "sizeof(";
            output += types.spelling(sizeof_expressions_.type[index]);
            output += ')';
            break;
        case ExprKind::initializer_list:
            if (auto const type{initializer_expressions_.type[index]}) {
                output += types.spelling(*type);
            }
            output += '{';
            append_arguments(initializer_expressions_.elements[index]);
            output += '}';
            break;
        case ExprKind::invalid:
            std::unreachable();
    }
    if (parenthesize) {
        output += ')';
    }
}
auto CppIR::render(Expr expression) const -> std::string {
    std::string result;
    append_expression(result, expression);
    return result;
}
void CppIR::collect_dependencies(Expr expression, std::vector<TypeDependencyId>& result) const {
    auto const index{expression.index()};
    auto append = [&](std::span<TypeDependencyId const> values) {
        result.insert(result.end(), values.begin(), values.end());
    };
    switch (expression.kind()) {
        case ExprKind::type_reference:
            append(types.dependencies(type_reference_expressions_.type[index]));
            break;
        case ExprKind::parenthesized:
            collect_dependencies(parenthesized_expressions_.operand[index], result);
            break;
        case ExprKind::template_id:
            collect_dependencies(template_id_expressions_.callee[index], result);
            for (auto const argument : expressions(template_id_expressions_.arguments[index])) {
                collect_dependencies(argument, result);
            }
            break;
        case ExprKind::named:
            append(dependency_list(named_expressions_.dependencies[index]));
            break;
        case ExprKind::raw:
            append(dependency_list(raw_expressions_.dependencies[index]));
            break;
        case ExprKind::call:
            collect_dependencies(call_expressions_.callee[index], result);
            for (auto const child : expressions(call_expressions_.arguments[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::member_access:
            collect_dependencies(member_expressions_.object[index], result);
            break;
        case ExprKind::subscript:
            collect_dependencies(subscript_expressions_.object[index], result);
            collect_dependencies(subscript_expressions_.index[index], result);
            break;
        case ExprKind::binary:
            collect_dependencies(binary_expressions_.left[index], result);
            collect_dependencies(binary_expressions_.right[index], result);
            break;
        case ExprKind::unary:
            collect_dependencies(unary_expressions_.operand[index], result);
            break;
        case ExprKind::static_cast_:
            append(types.dependencies(cast_expressions_.type[index]));
            collect_dependencies(cast_expressions_.operand[index], result);
            break;
        case ExprKind::sizeof_type:
            append(types.dependencies(sizeof_expressions_.type[index]));
            break;
        case ExprKind::initializer_list:
            if (auto const type{initializer_expressions_.type[index]}) {
                append(types.dependencies(*type));
            }
            for (auto const child : expressions(initializer_expressions_.elements[index])) {
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
