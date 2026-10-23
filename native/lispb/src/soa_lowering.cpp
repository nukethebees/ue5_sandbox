#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto mask_field_width(LoweringContext& context, SoaMemberId member) -> std::string {
    auto const& declarations{context.declarations};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const selected{declarations.items(members.mask_dimensions[members.index(member)])};
    if (selected.empty()) {
        return "1";
    }
    std::vector<std::string> extents;
    extents.reserve(selected.size());
    for (auto const dimension : selected) {
        extents.push_back("(" + context.text(dimensions.extent[dimensions.index(dimension)]) + ")");
    }
    return join(extents, " * ");
}

auto mask_index_expression(LoweringContext& context, SoaMemberId member) -> Expr {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const selected{declarations.items(members.mask_dimensions[members.index(member)])};
    auto const count{selected.size()};
    Expr result;
    for (std::size_t index{}; index < count; ++index) {
        auto term{ir.named(dimensions.index_name[dimensions.index(selected[index])])};
        for (auto extent_index{index + 1}; extent_index < count; ++extent_index) {
            term = ir.binary(
                BinaryOperator::multiply,
                term,
                ir.raw_expr(dimensions.extent[dimensions.index(selected[extent_index])]));
        }
        result = index == 0 ? term : ir.binary(BinaryOperator::add, result, term);
    }
    return result;
}

auto field_mask_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const row{soas.index(schema)};
    if (!soas.field_mask_name[row].has_value()) {
        return {};
    }

    auto const mask_name{*soas.field_mask_name[row]};
    auto const enum_name{context.text(*soas.field_enum_name[row])};
    auto const selected{declarations.items(soas.members[row])};
    auto const uint8_type{ir.types.intern("std::uint8_t", {ir.types.dependency("std::uint8_t", "cstdint")})};
    auto const int32_type{ir.types.intern("std::int32_t", {ir.types.dependency("std::int32_t", "cstdint")})};
    auto const uint64_type{ir.types.intern("std::uint64_t", {ir.types.dependency("std::uint64_t", "cstdint")})};
    auto const enum_type{ir.types.intern(enum_name)};
    auto const storage_type{ir.types.intern("storage_type")};
    std::vector<EnumValueId> values;
    std::string previous_name;
    std::string previous_width;
    auto next_value = [&] {
        return previous_name.empty() ? std::string{"0"}
                                     : "static_cast<std::uint8_t>(" + previous_name + ") + " + previous_width;
    };
    for (auto const member : selected) {
        auto const member_row{members.index(member)};
        if (!members.mask_field[member_row]) {
            continue;
        }
        auto const name{title_case_identifier(context.text(members.name[member_row]))};
        values.push_back(ir.enumerator(ir.store_text(name), ir.store_text(next_value())));
        previous_name = name;
        previous_width = mask_field_width(context, member);
    }
    values.push_back(ir.enumerator(ir.store_text("Count"), ir.store_text(next_value())));

    NodeListBuilder children{ir};
    children.add(ir.constexpr_member(int32_type, ir.store_text("field_count"),
                                    ir.static_cast_expr(int32_type, ir.named(enum_name + "::Count")), true))
        .add(ir.static_assertion(ir.store_text("field_count <= 64"), ir.store_text("Field mask exceeds 64 bits.")))
        .add(ir.using_declaration(ir.store_text("storage_type"), ir.types.intern(
            "std::conditional_t<field_count <= 8, std::uint8_t, "
            "std::conditional_t<field_count <= 16, std::uint16_t, "
            "std::conditional_t<field_count <= 32, std::uint32_t, std::uint64_t>>>",
            {ir.types.dependency("std::conditional_t", "type_traits"),
             ir.types.dependency("std::uint64_t", "cstdint")})))
        .new_lines(1);

    auto prepare = [&](FunctionId function) {
        ir.function_flags(function).set_is_constexpr(true);
        ir.set_function_noexcept(function);
        ir.function_formatting(function).body_layout = FunctionBodyLayout::compact;
        return function;
    };
    auto const default_constructor{prepare(ir.constructor(mask_name))};
    ir.set_function_defaulted(default_constructor);
    children.add(ir.declaration(default_constructor));
    auto const value_constructor{prepare(ir.constructor(mask_name, true))};
    ir.set_function_parameters(value_constructor, {ir.parameter("storage_type const", "value")});
    ir.set_function_body(value_constructor, {},
                         {ir.member_initializer(ir.store_text("value_"), ir.store_text("value"))});
    children.add(ir.header_function(value_constructor)).new_lines(1);

    auto method = [&](std::string_view name, CppTypeId return_type, FunctionReceiver receiver,
                      std::span<FunctionParameterId const> parameters, Nodes const& body,
                      bool nodiscard = true) {
        auto const function{prepare(ir.function(name, return_type))};
        ir.function_flags(function).set_is_nodiscard(nodiscard);
        ir.set_function_receiver(function, receiver);
        ir.set_function_parameters(function, parameters);
        ir.set_function_body(function, body);
        if (nodiscard) {
            ir.set_function_return_type(function, return_type, ReturnSyntax::trailing);
        }
        children.add(ir.header_function(function));
    };
    auto const field_parameter{ir.parameter(enum_name + " const", "field")};
    std::array const field_parameters{field_parameter};
    auto const field{ir.named("field")};
    auto const value{ir.named("value_")};
    auto const bit{ir.call(ir.named("bit"), {field})};
    method("value", storage_type, FunctionReceiver::const_, {}, {ir.return_stmt(value)});
    method("is_empty", ir.types.intern("bool"), FunctionReceiver::const_, {},
           {ir.return_stmt(ir.binary(BinaryOperator::equal, value, ir.literal("0")))});
    method("has", ir.types.intern("bool"), FunctionReceiver::const_, field_parameters,
           {ir.return_stmt(ir.binary(BinaryOperator::not_equal,
                                     ir.binary(BinaryOperator::bitwise_and, value, bit),
                                     ir.literal("0")))});
    method("index", int32_type, FunctionReceiver::static_, field_parameters,
           {ir.return_stmt(ir.static_cast_expr(int32_type, field))});
    method("set", ir.types.intern("void"), FunctionReceiver::unqualified, field_parameters,
           {ir.assignment(value, ir.static_cast_expr(storage_type,
               ir.binary(BinaryOperator::bitwise_or, value, bit)))}, false);
    std::array const mask_parameters{ir.parameter(context.text(mask_name) + " const", "fields")};
    method("set", ir.types.intern("void"), FunctionReceiver::unqualified, mask_parameters,
           {ir.assignment(value, ir.static_cast_expr(storage_type,
               ir.binary(BinaryOperator::bitwise_or, value, ir.member_access(ir.named("fields"), "value_"))))}, false);
    method("clear", ir.types.intern("void"), FunctionReceiver::unqualified, field_parameters,
           {ir.assignment(value, ir.static_cast_expr(storage_type,
               ir.binary(BinaryOperator::bitwise_and, value, ir.unary(UnaryOperator::bitwise_not, bit))))}, false);

    for (auto const member : selected) {
        auto const member_row{members.index(member)};
        auto const selected_dimensions{declarations.items(members.mask_dimensions[member_row])};
        if (!members.mask_field[member_row] || selected_dimensions.empty()) {
            continue;
        }
        auto const name{context.text(members.name[member_row])};
        std::vector<FunctionParameterId> parameters;
        for (auto const dimension : selected_dimensions) {
            parameters.push_back(ir.parameter(ir.types.intern("std::int32_t const"),
                dimensions.index_name[dimensions.index(dimension)]));
        }
        children.new_lines(1);
        method(name + "_field", enum_type, FunctionReceiver::static_, parameters,
               {ir.return_stmt(ir.static_cast_expr(enum_type,
                   ir.binary(BinaryOperator::add,
                       ir.static_cast_expr(int32_type, ir.named(enum_name + "::" + title_case_identifier(name))),
                       mask_index_expression(context, member))))});
    }

    children.add(ir.access(Access::private_));
    method("bit", storage_type, FunctionReceiver::static_, field_parameters,
           {ir.return_stmt(ir.static_cast_expr(storage_type,
               ir.binary(BinaryOperator::shift_left, ir.init_list({ir.literal("1")}, uint64_type),
                         ir.static_cast_expr(uint8_type, field))))});
    children.new_lines(1).add(ir.instance_member(storage_type, ir.store_text("value_"), ir.init_list({})));

    NodeListBuilder result{ir};
    result.add(ir.enumeration(*soas.field_enum_name[row], uint8_type, values)).new_lines(1)
        .add(ir.record(mask_name, children.build()))
        .add(ir.static_assertion(ir.store_text("sizeof(" + context.text(mask_name) + ") == sizeof(" +
                                               context.text(mask_name) + "::storage_type)"), {}))
        .add(ir.static_assertion(ir.store_text("std::is_trivially_copyable_v<" + context.text(mask_name) + ">"), {}));
    return result.build();
}

} // namespace

auto lower_soa_declaration(LoweringContext& context, DeclarationHandle schema)
    -> DeclarationEmission {
    auto& ir{context.ir};
    auto const& soas{context.declarations.soas};
    auto const row{soas.index(schema)};
    NodeListBuilder header{ir};
    auto const masks{field_mask_nodes(context, schema)};
    if (!masks.empty()) {
        header.new_lines(2).append(masks).new_lines(2);
    }
    if (soas.layout_only[row] && soas.const_view_functions[row].empty() &&
        soas.mutable_view_functions[row].empty() && !soas.equivalent_type[row]) {
        auto const type{context.semantic.find_declared(context.module, soas.name[row])};
        if (context.semantic.users_of(*type).empty()) {
            header.add(ir.namespace_node(
                ir.strings.intern(soa_namespace_name(context.text(soas.name[row]))),
                {emit_single_allocation_column_group(context, schema)}));
            return {.header = header.build(), .format_generated = true};
        }
    }
    auto lowered{lower_single_allocation(context, schema)};
    header.append(lowered.header);
    return {.header = header.build(), .source = lowered.source, .format_generated = true};
}

} // namespace lispb::codegen::detail
