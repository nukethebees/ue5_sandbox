#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir_builder.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

class StaticTableLowering : public CppIRBuilder {
  public:
    explicit StaticTableLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_static_table(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
        return {.header = {table_node(ctx, schema)}};
    }
  private:
    auto static_array_type(CppTypeId element_type) -> CppTypeId {
        return template_type(
            named_type("TStaticArray", {dependency("TStaticArray", "Containers/StaticArray.h")}),
            {element_type, named_type("num_rows")});
    }
    auto group_getter(LoweringContext& ctx, StaticTableGroupId group) -> NodeId {
        auto const& groups{ctx.declarations.static_table_groups};
        auto const row{groups.index(group)};
        auto const result_type{ctx.resolve(groups.type[row])};
        std::vector<Expr> arguments;
        arguments.reserve(groups.columns[row].size());
        for (auto const column : ctx.declarations.items(groups.columns[row])) {
            arguments.push_back(subscript(named(column), named("index")));
        }

        FunctionDefinition const created_function{
            .name = named("get_" + ctx.text(groups.name[row])),
            .return_type = result_type,
            .return_syntax = ReturnSyntax::trailing,
            .parameters = {parameter(
                intern_type("int32 const", {dependency("int32 const", "CoreMinimal.h")}), "index")},
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(init_list(arguments, result_type))}},
            .receiver = FunctionReceiver::const_,
            .flags = {.is_inline = true, .define_in_header = true}};
        return header_function(add_function(created_function));
    }
    auto table_node(LoweringContext& ctx, DeclarationHandle table) -> NodeId {
        auto const& tables{ctx.declarations.static_tables};
        auto const& column_schemas{ctx.declarations.static_table_columns};
        auto const& row_schemas{ctx.declarations.static_table_rows};
        auto const table_row{tables.index(table)};
        auto const rows{ctx.declarations.items(tables.rows[table_row])};
        auto const row_count{rows.size()};
        auto const columns{ctx.declarations.items(tables.columns[table_row])};
        auto const int32_type{intern_type("int32", {dependency("int32", "CoreMinimal.h")})};
        auto children{node_list()};
        children.add(constexpr_member(int32_type, store_text("num_rows"), literal(row_count)), 2);

        for (std::size_t index{0}; index < row_count; ++index) {
            auto const name{ctx.text(row_schemas.name[row_schemas.index(rows[index])])};
            children.add(constexpr_member(int32_type, store_text(name + "_index"), literal(index)),
                         index + 1 < row_count ? 1 : 2);
        }

        FunctionDefinition const num{
            .name = named("num"),
            .return_type = int32_type,
            .return_syntax = ReturnSyntax::trailing,
            .implementation = {.kind = FunctionImplementationKind::body,
                               .body = {return_stmt(named("num_rows"))}},
            .exception_specification = {.kind = FunctionExceptionKind::noexcept_},
            .receiver = FunctionReceiver::static_,
            .flags = {.is_constexpr = true, .is_inline = true, .define_in_header = true}};
        children.add(header_function(add_function(num)), 2);

        std::vector<std::string> column_names;
        column_names.reserve(columns.size());
        for (auto const column : columns) {
            column_names.push_back(ctx.text(column_schemas.name[column_schemas.index(column)]));
        }
        children.add(column_apply_arrays_function(ctx, column_names), 2)
            .add(column_apply_array_pairs_function(ctx, column_names), 2);

        for (auto const group : ctx.declarations.items(tables.groups[table_row])) {
            children.add(group_getter(ctx, group), 2);
        }

        for (auto const column : columns) {
            auto const column_row{column_schemas.index(column)};
            children.add(
                instance_member(static_array_type(ctx.resolve(column_schemas.type[column_row])),
                                column_schemas.name[column_row],
                                init_list({})),
                1);
        }

        auto const created_record{
            add_record({.name = named(tables.name[table_row]),
                        .children = children.build(),
                        .export_specifier = tables.export_specifier[table_row]})};
        return created_record;
    }
};

} // namespace

auto lower_static_table(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return StaticTableLowering{ctx.ir}.lower_static_table(ctx, schema);
}

} // namespace lispb::codegen::detail
