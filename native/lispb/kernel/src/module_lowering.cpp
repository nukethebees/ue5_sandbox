#include "lowering.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/unary_operator.h>
#include <kernel_codegen/aliasing.h>
#include <kernel_codegen/binary_operator.h>
#include <kernel_codegen/constant_kind.h>
#include <kernel_codegen/expression_id.h>
#include <kernel_codegen/expression_kind.h>
#include <kernel_codegen/indices.h>
#include <kernel_codegen/operation_kind.h>
#include <kernel_codegen/scalar_type.h>
#include <kernel_codegen/storage_kind.h>
#include <kernel_codegen/variant_kind.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {

auto parameter_type(lispb::codegen::CppIR& cpp,
                    Document const& document,
                    ExpandedVariantId const id,
                    std::size_t const index,
                    bool const raw) -> lispb::codegen::CppTypeId {
    auto const& expanded{document.expanded_variants};
    auto const type{standard_type(cpp.types, expanded.type(id))};
    if (expanded.storage(id)[index] == StorageKind::scalar) {
        return type;
    }
    auto const target{is_target(document, id, index)};
    if (raw) {
        return cpp.types.decorate(type, "", target ? "*" : " const*");
    }
    return cpp.types.template_type(cpp.types.std.span,
                                   {target ? type : cpp.types.decorate(type, "", " const")});
}

auto parameters(lispb::codegen::CppIR& cpp,
                Document const& document,
                ExpandedVariantId const id,
                bool const raw) -> std::vector<lispb::codegen::FunctionParameterId> {
    auto const& expanded{document.expanded_variants};
    std::vector<lispb::codegen::FunctionParameterId> result;
    auto const operation{expanded.operation(id)};
    auto const count{document.operations.operands(operation).size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const type{parameter_type(cpp, document, id, index, raw)};
        for (auto const& name : operand_names(document, id, index)) {
            result.push_back(cpp.parameter(type, name));
        }
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        auto const scalar{standard_type(cpp.types, expanded.type(id))};
        auto const type{raw ? cpp.types.decorate(scalar, "", "*")
                            : cpp.types.template_type(cpp.types.std.span, {scalar})};
        for (auto const& name : output_names(document, id)) {
            result.push_back(cpp.parameter(type, name));
        }
    }
    if (raw) {
        result.push_back(
            cpp.parameter(cpp.types.decorate(cpp.types.std.size_t, "", " const"), "count"));
    }
    return result;
}

auto lower_expression(lispb::codegen::CppIR& cpp,
                      ExpressionId const expression,
                      Document const& document,
                      ExpandedVariantId const id,
                      lispb::codegen::CppTypeId const concrete_type,
                      std::string_view const component) -> lispb::codegen::Expr {
    auto const& expanded{document.expanded_variants};
    switch (expression.kind()) {
        case ExpressionKind::reference: {
            auto const index{document.operand_index(expanded.operation(id),
                                                    document.expressions.operand(expression))};
            auto const operand{document.expressions.operand(expression)};
            auto name{document.text(document.operands.name(operand))};
            if (document.operands.is_component(operand)) {
                name += '_';
                name += component;
            }
            auto const reference{document.operands.is_component(operand)
                                     ? cpp.named(name)
                                     : cpp.named(document.operands.name(operand))};
            return expanded.storage(id)[index] == StorageKind::array
                     ? cpp.subscript(reference, cpp.named("i"))
                     : reference;
        }
        case ExpressionKind::literal:
            return cpp.static_cast_expr(concrete_type,
                                        cpp.literal(document.expressions.literal_id(expression)));
        case ExpressionKind::constant: {
            auto const limits{
                cpp.types.template_type(cpp.types.std.numeric_limits, {concrete_type})};
            auto const constant = [&](std::string_view name) {
                return cpp.call(cpp.type_reference(cpp.types.decorate(limits, "", name)));
            };
            if (document.expressions.constant(expression) == ConstantKind::nan) {
                return constant("::quiet_NaN");
            }
            auto const infinity{constant("::infinity")};
            if (document.expressions.constant(expression) == ConstantKind::infinity) {
                return infinity;
            }
            return cpp.unary(lispb::codegen::UnaryOperator::negate, infinity);
        }
        case ExpressionKind::binary: {
            using BinOp = lispb::codegen::BinaryOperator;
            BinOp operation{};
            switch (document.expressions.operation(expression)) {
                case BinaryOperator::add:
                    operation = BinOp::add;
                    break;
                case BinaryOperator::subtract:
                    operation = BinOp::subtract;
                    break;
                case BinaryOperator::multiply:
                    operation = BinOp::multiply;
                    break;
                case BinaryOperator::divide:
                    operation = BinOp::divide;
                    break;
            }
            auto const left{lower_expression(cpp,
                                             document.expressions.left(expression),
                                             document,
                                             id,
                                             concrete_type,
                                             component)};
            auto const right{lower_expression(cpp,
                                              document.expressions.right(expression),
                                              document,
                                              id,
                                              concrete_type,
                                              component)};
            return cpp.binary(operation, left, right);
        }
    }
    std::unreachable();
}

auto raw_body(lispb::codegen::CppIR& cpp, Document const& document, ExpandedVariantId const id)
    -> lispb::codegen::Nodes {
    auto const& expanded{document.expanded_variants};
    auto const operation{expanded.operation(id)};
    auto const concrete_type{standard_type(cpp.types, expanded.type(id))};
    auto const destinations{
        document.variants.kind(expanded.variant(id)) == VariantKind::in_place
            ? operand_names(document,
                            id,
                            document.operand_index(expanded.operation(id),
                                                   *document.variants.target(expanded.variant(id))))
            : output_names(document, id)};
    lispb::codegen::Nodes assignments;
    auto const count{destinations.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const component{operation.kind() == OperationKind::component_map
                                 ? document.text(document.operations.components(operation)[index])
                                 : std::string{}};
        auto const value{lower_expression(cpp,
                                          document.operations.expression(operation),
                                          document,
                                          id,
                                          concrete_type,
                                          component)};
        assignments.push_back(
            cpp.assignment(cpp.subscript(cpp.named(destinations[index]), cpp.named("i")), value));
    }
    auto const index{cpp.named("i")};
    auto const initializer{cpp.variable(cpp.types.std.size_t, "i")};
    auto const condition{
        cpp.binary(lispb::codegen::BinaryOperator::less, index, cpp.named("count"))};
    auto const increment{cpp.unary(lispb::codegen::UnaryOperator::preincrement, index)};
    return {cpp.for_stmt(initializer, condition, increment, cpp.block(std::move(assignments)))};
}

auto member_call(lispb::codegen::CppIR& cpp,
                 std::string_view const object,
                 std::string_view const member) -> lispb::codegen::Expr {
    return cpp.call(cpp.member_access(cpp.named(object), member));
}

auto raw_arguments(lispb::codegen::CppIR& cpp, Document const& document, ExpandedVariantId const id)
    -> std::vector<lispb::codegen::Expr> {
    auto const& expanded{document.expanded_variants};
    std::vector<lispb::codegen::Expr> result;
    auto const count{document.operations.operands(expanded.operation(id)).size()};
    for (std::size_t index{}; index < count; ++index) {
        for (auto const& name : operand_names(document, id, index)) {
            result.push_back(expanded.storage(id)[index] == StorageKind::array
                                 ? member_call(cpp, name, "data")
                                 : cpp.named(name));
        }
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        for (auto const& name : output_names(document, id)) {
            result.push_back(member_call(cpp, name, "data"));
        }
    }
    result.push_back(cpp.named("count"));
    return result;
}

auto checked_condition(lispb::codegen::CppIR& cpp,
                       lispb::codegen::Expr const condition,
                       std::string const& detail_namespace) -> lispb::codegen::NodeId {
    return cpp.expr_stmt(
        cpp.call(cpp.type_reference(cpp.types.named(detail_namespace + "::require")), {condition}));
}

auto overlap_check(lispb::codegen::CppIR& cpp,
                   std::string const& detail_namespace,
                   std::string const& lhs,
                   std::string const& rhs) -> lispb::codegen::NodeId {
    auto const overlaps{
        cpp.call(cpp.type_reference(cpp.types.named(detail_namespace + "::ranges_overlap")),
                 {member_call(cpp, lhs, "data"),
                  member_call(cpp, lhs, "size_bytes"),
                  member_call(cpp, rhs, "data"),
                  member_call(cpp, rhs, "size_bytes")})};
    auto const condition{cpp.unary(lispb::codegen::UnaryOperator::logical_not, overlaps)};
    return checked_condition(cpp, condition, detail_namespace);
}

auto public_body(lispb::codegen::CppIR& cpp,
                 Document const& document,
                 ExpandedVariantId const id,
                 std::string const& detail_namespace) -> lispb::codegen::Nodes {
    auto const& expanded{document.expanded_variants};
    lispb::codegen::Nodes output;
    auto const source{count_source(document, id)};
    output.push_back(cpp.variable("auto const", "count", member_call(cpp, source, "size")));
    auto const arrays{array_names(document, id)};
    for (auto const& array : arrays) {
        if (array == source) {
            continue;
        }
        auto const condition{cpp.binary(lispb::codegen::BinaryOperator::equal,
                                        member_call(cpp, array, "size"),
                                        cpp.named("count"))};
        output.push_back(checked_condition(cpp, condition, detail_namespace));
    }
    auto const operation{expanded.operation(id)};
    if (operation.kind() == OperationKind::component_map ||
        document.operations.aliasing(operation) == Aliasing::pairwise_disjoint) {
        auto const count{arrays.size()};
        for (std::size_t lhs{}; lhs < count; ++lhs) {
            for (auto rhs{lhs + 1}; rhs < count; ++rhs) {
                output.push_back(overlap_check(cpp, detail_namespace, arrays[lhs], arrays[rhs]));
            }
        }
    } else {
        auto const targets{document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place
                               ? output_names(document, id)
                               : operand_names(document,
                                               id,
                                               document.operand_index(expanded.operation(id),
                                                                      *document.variants.target(
                                                                          expanded.variant(id))))};
        for (auto const& target : targets) {
            for (auto const& array : arrays) {
                if (array != target) {
                    output.push_back(overlap_check(cpp, detail_namespace, target, array));
                }
            }
        }
    }
    auto const name{detail_namespace + "::" + raw_name(document, id)};
    output.push_back(cpp.expr_stmt(
        cpp.call(cpp.type_reference(cpp.types.named(name)), raw_arguments(cpp, document, id))));
    return output;
}

auto overlap_helper(lispb::codegen::CppIR& cpp) -> lispb::codegen::NodeId {
    using lispb::codegen::BinaryOperator;
    lispb::codegen::FunctionDefinition function{.name = cpp.named("ranges_overlap"),
                                                .return_type = cpp.types.std.bool_};
    auto const pointer_type{cpp.types.decorate(cpp.types.std.void_, "", " const* const")};
    auto const size_type{cpp.types.decorate(cpp.types.std.size_t, "", " const")};
    auto const address_type{cpp.types.std.uintptr_t};
    function.parameters = {cpp.parameter(pointer_type, "lhs"),
                           cpp.parameter(size_type, "lhs_size"),
                           cpp.parameter(pointer_type, "rhs"),
                           cpp.parameter(size_type, "rhs_size")};
    function.exception_specification = {.kind = lispb::codegen::FunctionExceptionKind::noexcept_};
    auto const lhs_size{cpp.named("lhs_size")};
    auto const rhs_size{cpp.named("rhs_size")};
    auto const lhs_address{cpp.named("lhs_address")};
    auto const rhs_address{cpp.named("rhs_address")};
    function.implementation = {
        .kind = lispb::codegen::FunctionImplementationKind::body,
        .body = {
            cpp.if_stmt(cpp.binary(BinaryOperator::logical_or,
                                   cpp.binary(BinaryOperator::equal, lhs_size, cpp.literal("0")),
                                   cpp.binary(BinaryOperator::equal, rhs_size, cpp.literal("0"))),
                        cpp.block({cpp.return_stmt(cpp.literal("false"))})),
            cpp.variable("auto const",
                         "lhs_address",
                         cpp.reinterpret_cast_expr(address_type, cpp.named("lhs"))),
            cpp.variable("auto const",
                         "rhs_address",
                         cpp.reinterpret_cast_expr(address_type, cpp.named("rhs"))),
            cpp.return_stmt(cpp.conditional(
                cpp.binary(BinaryOperator::less_equal, lhs_address, rhs_address),
                cpp.binary(BinaryOperator::less,
                           cpp.binary(BinaryOperator::subtract, rhs_address, lhs_address),
                           lhs_size),
                cpp.binary(BinaryOperator::less,
                           cpp.binary(BinaryOperator::subtract, lhs_address, rhs_address),
                           rhs_size)))}};
    return cpp.definition(cpp.add_function(function));
}

}

auto lower_module(lispb::codegen::CppIR& cpp,
                  ModuleId const module,
                  EmissionId const emission,
                  Document const& document) -> lispb::codegen::ModuleId {
    auto const& expanded{document.expanded_variants};
    auto const cpp_namespace{document.emissions.cpp_namespace(emission)};
    auto const detail_namespace{document.text(document.modules.name(module)) + "_detail"};
    auto const void_type{cpp.types.std.void_};
    lispb::codegen::Nodes declarations;
    lispb::codegen::Nodes definitions;
    lispb::codegen::Nodes implementation;
    lispb::codegen::FunctionDefinition invariant_failed{.name = cpp.named("invariant_failed"),
                                                        .return_type = void_type,
                                                        .flags = {.is_noreturn = true}};
    invariant_failed.exception_specification = {
        .kind = lispb::codegen::FunctionExceptionKind::noexcept_};
    invariant_failed.implementation = {
        .kind = lispb::codegen::FunctionImplementationKind::body,
        .body = {cpp.expr_stmt(cpp.call(cpp.type_reference(cpp.types.std.abort)))}};
    implementation.push_back(cpp.definition(cpp.add_function(invariant_failed)));
    lispb::codegen::FunctionDefinition require{.name = cpp.named("require"),
                                               .return_type = cpp.types.std.void_};
    require.parameters = {
        cpp.parameter(cpp.types.decorate(cpp.types.std.bool_, "", " const"), "condition")};
    require.exception_specification = {.kind = lispb::codegen::FunctionExceptionKind::noexcept_};
    require.implementation = {
        .kind = lispb::codegen::FunctionImplementationKind::body,
        .body = {cpp.if_stmt(
            cpp.unary(lispb::codegen::UnaryOperator::logical_not, cpp.named("condition")),
            cpp.block({cpp.expr_stmt(cpp.call(cpp.named("invariant_failed"), {}))}))}};
    implementation.push_back(cpp.definition(cpp.add_function(require)));
    implementation.push_back(overlap_helper(cpp));

    auto const range{document.modules.expanded_variants(module)};
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        lispb::codegen::FunctionDefinition raw_function{.name = cpp.named(raw_name(document, id)),
                                                        .return_type = void_type};
        raw_function.parameters = parameters(cpp, document, id, true);
        raw_function.implementation = {.kind = lispb::codegen::FunctionImplementationKind::body,
                                       .body = raw_body(cpp, document, id)};
        raw_function.exception_specification = {
            .kind = lispb::codegen::FunctionExceptionKind::noexcept_};
        implementation.push_back(cpp.definition(cpp.add_function(raw_function)));

        auto function_parameters{parameters(cpp, document, id, false)};
        auto function{lispb::codegen::FunctionDefinition{
            .name = cpp.named(document.variants.name(expanded.variant(id))),
            .return_type = void_type}};
        function.parameters = std::move(function_parameters);
        function.implementation = {.kind = lispb::codegen::FunctionImplementationKind::body,
                                   .body = public_body(cpp, document, id, detail_namespace)};
        function.exception_specification = {.kind =
                                                lispb::codegen::FunctionExceptionKind::noexcept_};
        auto const published{cpp.add_function(function)};
        declarations.push_back(cpp.declaration(published));
        definitions.push_back(cpp.definition(published));
    }

    lispb::codegen::NodeListBuilder header_nodes{cpp};
    header_nodes.add(cpp.include_dependencies(), 2);
    header_nodes.add(cpp.namespace_node(cpp_namespace, declarations));
    auto const header{cpp.files.append(document.emissions.header(emission), header_nodes.build())};
    cpp.files.flags[cpp.files.index(header)].set_format_generated(true);

    lispb::codegen::NodeListBuilder source_nodes{cpp};
    source_nodes.add(cpp.include(document.emissions.header_include(emission),
                                 lispb::codegen::IncludeStyle::quoted),
                     2);
    source_nodes.add(cpp.include_dependencies());
    source_nodes.new_lines(2)
        .add(cpp.namespace_node(cpp.strings.intern(detail_namespace), implementation), 2)
        .add(cpp.namespace_node(cpp_namespace, definitions));
    auto const source{cpp.files.append(document.emissions.source(emission), source_nodes.build())};
    auto& source_flags{cpp.files.flags[cpp.files.index(source)]};
    source_flags.set_pragma_once(false);
    source_flags.set_format_generated(true);
    return cpp.modules.append(document.modules.name(module), header, source);
}

}
