#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir_builder.h>
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

class SoaLowering : public CppIRBuilder {
  public:
    explicit SoaLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_soa_declaration(LoweringContext& ctx, DeclarationHandle schema)
        -> DeclarationEmission {
        auto const& soas{ctx.declarations.soas};
        auto const row{soas.index(schema)};
        auto header{node_list()};
        auto const masks{field_mask_nodes(ctx, schema)};
        if (!masks.empty()) {
            header.new_lines(2).append(masks).new_lines(2);
        }
        if (soas.layout_only[row] && soas.const_view_functions[row].empty() &&
            soas.mutable_view_functions[row].empty() && !soas.equivalent_type[row]) {
            auto const type{ctx.semantic.find_declared(ctx.module, soas.name[row])};
            if (ctx.semantic.users_of(*type).empty()) {
                header.add(namespace_node(store_text(soa_namespace_name(ctx.text(soas.name[row]))),
                                          emit_single_allocation_layout(
                                              ctx, build_single_allocation_model(ctx, schema))));
                return {.header = header.build(), .format_generated = true};
            }
        }
        auto lowered{lower_single_allocation(ctx, schema)};
        header.append(lowered.header);
        return {.header = header.build(), .source = lowered.source, .format_generated = true};
    }
  private:
    auto mask_field_width(LoweringContext& ctx, SoaMemberId member) -> std::string {
        auto const& decls{ctx.declarations};
        auto const& members{decls.soa_members};
        auto const& dimensions{decls.soa_mask_dimensions};
        auto const selected{decls.items(members.mask_dimensions[members.index(member)])};
        if (selected.empty()) {
            return "1";
        }
        std::vector<std::string> extents;
        extents.reserve(selected.size());
        for (auto const dimension : selected) {
            extents.push_back("(" + ctx.text(dimensions.extent[dimensions.index(dimension)]) + ")");
        }
        return join(extents, " * ");
    }
    auto mask_index_expression(LoweringContext& ctx, SoaMemberId member) -> Expr {
        auto const& decls{ctx.declarations};
        auto const& members{decls.soa_members};
        auto const& dimensions{decls.soa_mask_dimensions};
        auto const selected{decls.items(members.mask_dimensions[members.index(member)])};
        auto const count{selected.size()};
        Expr result;
        for (std::size_t index{}; index < count; ++index) {
            auto term{named(dimensions.index_name[dimensions.index(selected[index])])};
            for (auto extent_index{index + 1}; extent_index < count; ++extent_index) {
                term =
                    binary(BinaryOperator::multiply,
                           term,
                           raw_expr(dimensions.extent[dimensions.index(selected[extent_index])]));
            }
            result = index == 0 ? term : binary(BinaryOperator::add, result, term);
        }
        return result;
    }
    auto field_mask_nodes(LoweringContext& ctx, DeclarationHandle schema) -> Nodes {
        auto const& decls{ctx.declarations};
        auto const& soas{decls.soas};
        auto const& members{decls.soa_members};
        auto const& dimensions{decls.soa_mask_dimensions};
        auto const row{soas.index(schema)};
        if (!soas.field_mask_name[row].has_value()) {
            return {};
        }

        auto const mask_name{*soas.field_mask_name[row]};
        auto const enum_name{ctx.text(*soas.field_enum_name[row])};
        auto const selected{decls.items(soas.members[row])};
        auto const uint8_type{std_types().u8};
        auto const int32_type{std_types().i32};
        auto const enum_type{intern_type(enum_name)};
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
            auto const name{title_case_identifier(ctx.text(members.name[member_row]))};
            values.push_back(enumerator(store_text(name), store_text(next_value())));
            previous_name = name;
            previous_width = mask_field_width(ctx, member);
        }
        values.push_back(enumerator(store_text("Count"), store_text(next_value())));

        auto const field_count{compose_type({{.text = store_text("static_cast<")},
                                             {.type = std_types().size_t},
                                             {.text = store_text(">(")},
                                             {.type = decorate_type(enum_type, "", "::Count")},
                                             {.text = store_text(")")}})};
        auto const base{
            template_type(named_type("ioj::FieldMask",
                                     NameOrigin::global,
                                     {dependency("ioj::FieldMask", "ioj/field_mask.h")}),
                          {enum_type, field_count})};
        auto children{node_list()};
        children.add(using_declaration(store_text("Base"), base))
            .add(using_import(named_type("Base::Base")));

        for (auto const member : selected) {
            auto const member_row{members.index(member)};
            auto const selected_dimensions{decls.items(members.mask_dimensions[member_row])};
            if (!members.mask_field[member_row] || selected_dimensions.empty()) {
                continue;
            }
            auto const name{ctx.text(members.name[member_row])};
            std::vector<FunctionParameterId> parameters;
            for (auto const dimension : selected_dimensions) {
                parameters.push_back(parameter(decorate_type(std_types().i32, "", " const"),
                                               dimensions.index_name[dimensions.index(dimension)]));
            }
            FunctionDefinition function_id{.name = named(name + "_field"),
                                           .return_type = enum_type};
            auto& flags{function_id.flags};
            flags.set_is_constexpr(true);
            flags.set_define_in_header(true);
            flags.set_is_nodiscard(true);
            function_id.exception_specification = {.kind = FunctionExceptionKind::noexcept_};
            function_id.formatting.body_layout = FunctionBodyLayout::compact;
            function_id.receiver = FunctionReceiver::static_;
            function_id.parameters.assign(parameters.begin(), parameters.end());
            function_id.return_type = enum_type;
            function_id.return_syntax = ReturnSyntax::trailing;
            function_id.implementation = {
                .kind = FunctionImplementationKind::body,
                .body = {return_stmt(static_cast_expr(
                    enum_type,
                    binary(
                        BinaryOperator::add,
                        static_cast_expr(int32_type,
                                         type_reference(decorate_type(
                                             enum_type, "", "::" + title_case_identifier(name)))),
                        mask_index_expression(ctx, member))))}};
            children.new_lines(1).add(header_function(add_function(function_id)));
        }

        auto const mask{record(mask_name, children.build())};
        set_record_bases(mask, {base});

        auto const mask_type{named_type(ctx.text(mask_name))};
        auto result{node_list()};
        result.add(enumeration(*soas.field_enum_name[row], uint8_type, values))
            .new_lines(1)
            .add(mask)
            .add(static_assertion(
                binary(BinaryOperator::equal,
                       sizeof_type(mask_type),
                       sizeof_type(decorate_type(mask_type, "", "::storage_type")))))
            .add(static_assertion(
                type_reference(template_type(std_types().is_trivially_copyable_v, {mask_type}))));
        return result.build();
    }
};

} // namespace

auto lower_soa_declaration(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return SoaLowering{ctx.ir}.lower_soa_declaration(ctx, schema);
}

} // namespace lispb::codegen::detail
