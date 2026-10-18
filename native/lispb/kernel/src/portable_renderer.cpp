#include "portable_renderer.h"

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
#include <lispb/kernel_profile.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel_codegen::detail {
namespace {

auto parameter_type(Document const& document,
                    ExpandedVariantId const id,
                    std::size_t const index,
                    bool const standard,
                    bool const raw,
                    bool const owning) -> std::string {
    auto const& expanded{document.expanded_variants};
    auto const type{standard ? standard_type(expanded.type(id)) : type_spelling(expanded.type(id))};
    if (expanded.storage(id)[index] == StorageKind::scalar) {
        return type + (standard ? "" : " const");
    }
    auto const target{is_target(document, id, index)};
    if (raw) {
        auto result{type + (target ? "*" : " const*")};
        if (!standard && (target || document.operations.aliasing(expanded.operation(id)) ==
                                        Aliasing::pairwise_disjoint)) {
            result += " RESTRICT";
        }
        return result;
    }
    if (standard) {
        return "std::span<" + type + (target ? ">" : " const>");
    }
    if (target) {
        return owning ? "TArray<" + type + ">&" : "TArrayView<" + type + "> const";
    }
    return "TConstArrayView<" + type + "> const";
}

auto parameters(lispb::codegen::CppIR& cpp,
                Document const& document,
                ExpandedVariantId const id,
                bool const standard,
                bool const raw,
                bool const owning = false) -> std::vector<lispb::codegen::FunctionParameterId> {
    auto const& expanded{document.expanded_variants};
    std::vector<lispb::codegen::FunctionParameterId> result;
    auto const operation{expanded.operation(id)};
    auto const count{document.operations.operands(operation).size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const type{
            cpp.types.intern(parameter_type(document, id, index, standard, raw, owning))};
        for (auto const& name : operand_names(document, id, index)) {
            result.push_back(cpp.parameter(type, name));
        }
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        auto const scalar{standard ? standard_type(expanded.type(id))
                                   : type_spelling(expanded.type(id))};
        auto const spelling{raw        ? scalar + (standard ? "*" : "* RESTRICT")
                            : standard ? "std::span<" + scalar + ">"
                                       : "TArrayView<" + scalar + "> const"};
        auto const type{cpp.types.intern(spelling)};
        for (auto const& name : output_names(document, id)) {
            result.push_back(cpp.parameter(type, name));
        }
    }
    if (raw) {
        result.push_back(cpp.parameter(standard ? "std::size_t const" : "int32 const", "count"));
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
            auto const type{std::string{cpp.types.spelling(concrete_type)}};
            auto const prefix{"std::numeric_limits<" + type + ">::"};
            if (document.expressions.constant(expression) == ConstantKind::nan) {
                return cpp.call(cpp.named(prefix + "quiet_NaN"));
            }
            auto const infinity{cpp.call(cpp.named(prefix + "infinity"))};
            if (document.expressions.constant(expression) == ConstantKind::infinity) {
                return infinity;
            }
            return cpp.raw_expr("(-" + prefix + "infinity())");
        }
        case ExpressionKind::binary: {
            lispb::codegen::BinaryOperator operation{};
            switch (document.expressions.operation(expression)) {
                case BinaryOperator::add:
                    operation = lispb::codegen::BinaryOperator::add;
                    break;
                case BinaryOperator::subtract:
                    operation = lispb::codegen::BinaryOperator::subtract;
                    break;
                case BinaryOperator::multiply:
                    operation = lispb::codegen::BinaryOperator::multiply;
                    break;
                case BinaryOperator::divide:
                    operation = lispb::codegen::BinaryOperator::divide;
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

auto raw_body(lispb::codegen::CppIR& cpp,
              Document const& document,
              ExpandedVariantId const id,
              bool const standard) -> lispb::codegen::Nodes {
    auto const& expanded{document.expanded_variants};
    auto const operation{expanded.operation(id)};
    auto const concrete_type{cpp.types.intern(standard ? standard_type(expanded.type(id))
                                                       : type_spelling(expanded.type(id)))};
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
    auto const initializer{
        cpp.variable(standard ? "std::size_t" : "int32",
                     "i",
                     standard ? std::optional<lispb::codegen::Expr>{} : cpp.literal("0"))};
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

auto raw_arguments(lispb::codegen::CppIR& cpp,
                   Document const& document,
                   ExpandedVariantId const id,
                   bool const standard) -> std::vector<lispb::codegen::Expr> {
    auto const& expanded{document.expanded_variants};
    std::vector<lispb::codegen::Expr> result;
    auto const count{document.operations.operands(expanded.operation(id)).size()};
    for (std::size_t index{}; index < count; ++index) {
        for (auto const& name : operand_names(document, id, index)) {
            result.push_back(expanded.storage(id)[index] == StorageKind::array
                                 ? member_call(cpp, name, standard ? "data" : "GetData")
                                 : cpp.named(name));
        }
    }
    if (document.variants.kind(expanded.variant(id)) == VariantKind::out_of_place) {
        for (auto const& name : output_names(document, id)) {
            result.push_back(member_call(cpp, name, standard ? "data" : "GetData"));
        }
    }
    result.push_back(cpp.named("count"));
    return result;
}

auto checked_condition(lispb::codegen::CppIR& cpp,
                       lispb::codegen::Expr const condition,
                       std::string const& detail_namespace,
                       bool const standard,
                       std::string_view const message) -> lispb::codegen::NodeId {
    if (standard) {
        return cpp.expr_stmt(cpp.call(cpp.named(detail_namespace + "::require"), {condition}));
    }
    auto const text{cpp.call(cpp.named("TEXT"), {cpp.string_literal(message)})};
    return cpp.expr_stmt(cpp.call(cpp.named("checkf"), {condition, text}));
}

auto overlap_check(lispb::codegen::CppIR& cpp,
                   Document const& document,
                   ExpandedVariantId const id,
                   std::string const& detail_namespace,
                   bool const standard,
                   std::string const& lhs,
                   std::string const& rhs) -> lispb::codegen::NodeId {
    auto const& expanded{document.expanded_variants};
    auto const size = [&](std::string_view const name) {
        if (standard) {
            return member_call(cpp, name, "size_bytes");
        }
        auto const count{
            cpp.static_cast_expr(cpp.types.intern("std::size_t"), member_call(cpp, name, "Num"))};
        auto const element_size{
            cpp.sizeof_type(cpp.types.intern(type_spelling(expanded.type(id))))};
        return cpp.binary(lispb::codegen::BinaryOperator::multiply, count, element_size);
    };
    auto const name{standard ? detail_namespace + "::ranges_overlap" : "ranges_overlap"};
    auto const overlaps{cpp.call(cpp.named(name),
                                 {member_call(cpp, lhs, standard ? "data" : "GetData"),
                                  size(lhs),
                                  member_call(cpp, rhs, standard ? "data" : "GetData"),
                                  size(rhs)})};
    auto const condition{cpp.unary(lispb::codegen::UnaryOperator::logical_not, overlaps)};
    auto const message{document.text(document.variants.name(expanded.variant(id))) + ": " + lhs +
                       " and " + rhs + " must not overlap"};
    return checked_condition(cpp, condition, detail_namespace, standard, message);
}

auto public_body(lispb::codegen::CppIR& cpp,
                 Document const& document,
                 ExpandedVariantId const id,
                 std::string const& detail_namespace,
                 bool const standard,
                 bool const owning) -> lispb::codegen::Nodes {
    auto const& expanded{document.expanded_variants};
    lispb::codegen::Nodes output;
    if (owning) {
        std::vector<lispb::codegen::Expr> arguments;
        auto const view_type{
            cpp.types.intern("TArrayView<" + type_spelling(expanded.type(id)) + ">")};
        auto const count{document.operations.operands(expanded.operation(id)).size()};
        for (std::size_t index{}; index < count; ++index) {
            auto const name{document.text(document.operands.name(
                document.operations.operands(expanded.operation(id))[index]))};
            arguments.push_back(is_target(document, id, index)
                                    ? cpp.init_list({cpp.named(name)}, view_type)
                                    : cpp.named(name));
        }
        auto const function{cpp.named(document.variants.name(expanded.variant(id)))};
        return {cpp.expr_stmt(cpp.call(function, arguments))};
    }

    auto const source{count_source(document, id)};
    output.push_back(cpp.variable(standard ? "auto const" : "int32 const",
                                  "count",
                                  member_call(cpp, source, standard ? "size" : "Num")));
    auto const arrays{array_names(document, id)};
    for (auto const& array : arrays) {
        if (array == source) {
            continue;
        }
        auto const condition{cpp.binary(lispb::codegen::BinaryOperator::equal,
                                        member_call(cpp, array, standard ? "size" : "Num"),
                                        cpp.named("count"))};
        auto const message{document.text(document.variants.name(expanded.variant(id))) + ": " +
                           array + " and " + source + " must have equal lengths"};
        output.push_back(checked_condition(cpp, condition, detail_namespace, standard, message));
    }
    auto const operation{expanded.operation(id)};
    if (operation.kind() == OperationKind::component_map ||
        document.operations.aliasing(operation) == Aliasing::pairwise_disjoint) {
        auto const count{arrays.size()};
        for (std::size_t lhs{}; lhs < count; ++lhs) {
            for (auto rhs{lhs + 1}; rhs < count; ++rhs) {
                output.push_back(overlap_check(
                    cpp, document, id, detail_namespace, standard, arrays[lhs], arrays[rhs]));
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
                    output.push_back(overlap_check(
                        cpp, document, id, detail_namespace, standard, target, array));
                }
            }
        }
    }
    auto const name{(standard ? detail_namespace + "::" : "") + raw_name(document, id)};
    output.push_back(
        cpp.expr_stmt(cpp.call(cpp.named(name), raw_arguments(cpp, document, id, standard))));
    return output;
}

auto overlap_helper() -> std::string_view {
    return R"(auto ranges_overlap(void const* const lhs, std::size_t const lhs_size,
                    void const* const rhs, std::size_t const rhs_size) noexcept -> bool {
    if (lhs_size == 0 || rhs_size == 0) {
        return false;
    }

    auto const lhs_address{reinterpret_cast<std::uintptr_t>(lhs)};
    auto const rhs_address{reinterpret_cast<std::uintptr_t>(rhs)};
    return lhs_address <= rhs_address ? rhs_address - lhs_address < lhs_size
                                      : lhs_address - rhs_address < rhs_size;
})";
}

}

auto lower_portable(lispb::codegen::CppIR& cpp,
                    ModuleId const module,
                    EmissionId const emission,
                    Document const& document) -> lispb::codegen::ModuleId {
    auto const& expanded{document.expanded_variants};
    auto const standard{document.emissions.profile(emission) == lispb::KernelProfile::standard};
    auto const cpp_namespace{document.emissions.cpp_namespace(emission)};
    auto const detail_namespace{standard ? document.text(document.modules.name(module)) + "_detail"
                                         : std::string{}};
    auto const void_type{cpp.types.intern("void")};
    lispb::codegen::Nodes declarations;
    lispb::codegen::Nodes definitions;
    lispb::codegen::Nodes implementation;
    if (standard) {
        implementation.push_back(
            cpp.raw("[[noreturn]] void invariant_failed() noexcept { std::abort(); }\n\n"
                    "void require(bool const condition) noexcept {\n"
                    "    if (!condition) {\n        invariant_failed();\n    }\n}"));
    }
    implementation.push_back(cpp.raw(overlap_helper()));

    auto const range{document.modules.expanded_variants(module)};
    auto const end{range.offset + range.count};
    for (auto variant_index{range.offset}; variant_index < end; ++variant_index) {
        auto const id{ExpandedVariantId{variant_index}};
        auto const raw_function{cpp.function(raw_name(document, id), void_type)};
        cpp.set_function_parameters(raw_function, parameters(cpp, document, id, standard, true));
        cpp.set_function_body(raw_function, raw_body(cpp, document, id, standard));
        cpp.set_function_noexcept(raw_function);
        implementation.push_back(cpp.definition(raw_function));

        auto const overload_count{
            !standard && document.variants.kind(expanded.variant(id)) == VariantKind::in_place ? 2
                                                                                               : 1};
        for (int overload{}; overload < overload_count; ++overload) {
            auto function_parameters{parameters(cpp, document, id, standard, false, overload != 0)};
            auto const function{
                cpp.function(document.variants.name(expanded.variant(id)), void_type)};
            cpp.set_function_parameters(function, std::move(function_parameters));
            cpp.set_function_body(
                function,
                public_body(cpp, document, id, detail_namespace, standard, overload != 0));
            cpp.set_function_noexcept(function);
            cpp.set_function_export_specifier(
                function,
                standard ? lispb::IdentifierId{} : document.emissions.export_specifier(emission));
            cpp.function_formatting(function) = {.export_after_return_type = true};
            declarations.push_back(cpp.declaration(function));
            definitions.push_back(cpp.definition(function));
        }
    }

    lispb::codegen::NodeListBuilder header_nodes{cpp};
    if (standard) {
        header_nodes
            .add(cpp.include(cpp.strings.add("cstdint"), lispb::codegen::IncludeStyle::system))
            .add(cpp.include(cpp.strings.add("span"), lispb::codegen::IncludeStyle::system), 2);
    } else {
        header_nodes
            .add(cpp.include(cpp.strings.add("Containers/Array.h"),
                             lispb::codegen::IncludeStyle::quoted))
            .add(cpp.include(cpp.strings.add("Containers/ArrayView.h"),
                             lispb::codegen::IncludeStyle::quoted))
            .add(cpp.include(cpp.strings.add("CoreTypes.h"), lispb::codegen::IncludeStyle::quoted),
                 2);
    }
    header_nodes.add(cpp.namespace_node(cpp_namespace, declarations));
    auto const header{cpp.files.append(document.emissions.header(emission), header_nodes.build())};
    cpp.files.flags[cpp.files.index(header)].set_format_generated(true);

    lispb::codegen::NodeListBuilder source_nodes{cpp};
    source_nodes.add(cpp.include(document.emissions.header_include(emission),
                                 lispb::codegen::IncludeStyle::quoted),
                     2);
    if (!standard) {
        source_nodes.add(
            cpp.include(cpp.strings.add("CoreMinimal.h"), lispb::codegen::IncludeStyle::quoted), 2);
    }
    source_nodes.add(cpp.include(cpp.strings.add("cstddef"), lispb::codegen::IncludeStyle::system))
        .add(cpp.include(cpp.strings.add("cstdint"), lispb::codegen::IncludeStyle::system));
    if (standard) {
        source_nodes.add(
            cpp.include(cpp.strings.add("cstdlib"), lispb::codegen::IncludeStyle::system));
    }
    if (std::ranges::any_of(document.modules.operations(module), [&](auto const operation) {
            return contains_constant(document.expressions,
                                     document.operations.expression(operation));
        })) {
        source_nodes.add(
            cpp.include(cpp.strings.add("limits"), lispb::codegen::IncludeStyle::system));
    }
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
