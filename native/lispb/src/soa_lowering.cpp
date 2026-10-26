#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_body_layout.h>
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

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto mask_field_width(LoweringContext& context, SoaMemberId member) -> std::string {
    auto const& decls{context.declarations};
    auto const& members{decls.soa_members};
    auto const& dimensions{decls.soa_mask_dimensions};
    auto const selected{decls.items(members.mask_dimensions[members.index(member)])};
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
    auto const& decls{context.declarations};
    auto const& members{decls.soa_members};
    auto const& dimensions{decls.soa_mask_dimensions};
    auto const selected{decls.items(members.mask_dimensions[members.index(member)])};
    auto const count{selected.size()};
    Expr result;
    for (std::size_t index{}; index < count; ++index) {
        auto term{ir.named(dimensions.index_name[dimensions.index(selected[index])])};
        for (auto extent_index{index + 1}; extent_index < count; ++extent_index) {
            term =
                ir.binary(BinaryOperator::multiply,
                          term,
                          ir.raw_expr(dimensions.extent[dimensions.index(selected[extent_index])]));
        }
        result = index == 0 ? term : ir.binary(BinaryOperator::add, result, term);
    }
    return result;
}

auto field_mask_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    auto const& decls{context.declarations};
    auto const& soas{decls.soas};
    auto const& members{decls.soa_members};
    auto const& dimensions{decls.soa_mask_dimensions};
    auto const row{soas.index(schema)};
    if (!soas.field_mask_name[row].has_value()) {
        return {};
    }

    auto const mask_name{*soas.field_mask_name[row]};
    auto const enum_name{context.text(*soas.field_enum_name[row])};
    auto const selected{decls.items(soas.members[row])};
    auto const uint8_type{
        ir.types.intern("std::uint8_t", {ir.types.dependency("std::uint8_t", "cstdint")})};
    auto const int32_type{
        ir.types.intern("std::int32_t", {ir.types.dependency("std::int32_t", "cstdint")})};
    auto const enum_type{ir.types.intern(enum_name)};
    std::vector<EnumValueId> values;
    std::string previous_name;
    std::string previous_width;
    auto next_value = [&] {
        return previous_name.empty()
                 ? std::string{"0"}
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

    auto const base{ir.types.template_type(
        ir.types.intern("ioj::FieldMask",
                        {ir.types.dependency("ioj::FieldMask", "ioj/field_mask.h")}),
        {enum_type, ir.types.intern("static_cast<std::size_t>(" + enum_name + "::Count)")})};
    NodeListBuilder children{ir};
    children.add(ir.using_declaration(ir.store_text("Base"), base))
        .add(ir.raw("using Base::Base;"));

    for (auto const member : selected) {
        auto const member_row{members.index(member)};
        auto const selected_dimensions{decls.items(members.mask_dimensions[member_row])};
        if (!members.mask_field[member_row] || selected_dimensions.empty()) {
            continue;
        }
        auto const name{context.text(members.name[member_row])};
        std::vector<FunctionParameterId> parameters;
        for (auto const dimension : selected_dimensions) {
            parameters.push_back(ir.parameter(ir.types.intern("std::int32_t const"),
                                              dimensions.index_name[dimensions.index(dimension)]));
        }
        auto const function{ir.function(name + "_field", enum_type)};
        auto& flags{ir.function_flags(function)};
        flags.set_is_constexpr(true);
        flags.set_define_in_header(true);
        flags.set_is_nodiscard(true);
        ir.set_function_noexcept(function);
        ir.function_formatting(function).body_layout = FunctionBodyLayout::compact;
        ir.set_function_receiver(function, FunctionReceiver::static_);
        ir.set_function_parameters(function, parameters);
        ir.set_function_return_type(function, enum_type, ReturnSyntax::trailing);
        ir.set_function_body(
            function,
            {ir.return_stmt(ir.static_cast_expr(
                enum_type,
                ir.binary(BinaryOperator::add,
                          ir.static_cast_expr(
                              int32_type, ir.named(enum_name + "::" + title_case_identifier(name))),
                          mask_index_expression(context, member))))});
        children.new_lines(1).add(ir.header_function(function));
    }

    auto const mask{ir.record(mask_name, children.build())};
    ir.set_record_bases(mask, {base});

    NodeListBuilder result{ir};
    result.add(ir.enumeration(*soas.field_enum_name[row], uint8_type, values))
        .new_lines(1)
        .add(mask)
        .add(
            ir.static_assertion(ir.store_text("sizeof(" + context.text(mask_name) + ") == sizeof(" +
                                              context.text(mask_name) + "::storage_type)"),
                                {}))
        .add(ir.static_assertion(
            ir.store_text("std::is_trivially_copyable_v<" + context.text(mask_name) + ">"), {}));
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
                emit_single_allocation_layout(context,
                                              build_single_allocation_model(context, schema))));
            return {.header = header.build(), .format_generated = true};
        }
    }
    auto lowered{lower_single_allocation(context, schema)};
    header.append(lowered.header);
    return {.header = header.build(), .source = lowered.source, .format_generated = true};
}

} // namespace lispb::codegen::detail
