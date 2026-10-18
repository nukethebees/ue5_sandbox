#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
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

auto static_array_type(CppIR& ir, CppTypeId element_type) -> CppTypeId {
    auto const nested{ir.types.dependencies(element_type)};
    std::vector<TypeDependencyId> dependencies{nested.begin(), nested.end()};
    dependencies.push_back(ir.types.dependency("TStaticArray", "Containers/StaticArray.h"));
    return ir.types.intern("TStaticArray<" + std::string{ir.types.spelling(element_type)} +
                               ", num_rows>",
                           dependencies);
}

auto group_getter(LoweringContext& context, StaticTableGroupId group) -> NodeId {
    auto& ir{context.ir};
    auto const& groups{context.declarations.static_table_groups};
    auto const row{groups.index(group)};
    auto const result_type{context.resolve(groups.type[row])};
    std::vector<Expr> arguments;
    arguments.reserve(groups.columns[row].size());
    for (auto const column : context.declarations.items(groups.columns[row])) {
        arguments.push_back(ir.subscript(ir.named(column), ir.named("index")));
    }

    auto const created_function{ir.function("get_" + context.text(groups.name[row]), "auto")};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(
            ir.types.intern("int32 const", {ir.types.dependency("int32 const", "CoreMinimal.h")}),
            "index")});
    ir.set_function_body(created_function, {ir.return_stmt(ir.init_list(arguments, result_type))});
    ir.set_function_return_type(created_function, result_type, ReturnSyntax::trailing);
    ir.set_function_receiver(created_function, FunctionReceiver::const_);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    return ir.header_function(created_function);
}

auto table_node(LoweringContext& context, DeclarationHandle table) -> NodeId {
    auto& ir{context.ir};
    auto const rows{context.declarations.items(
        context.declarations.static_tables.rows[context.declarations.static_tables.index(table)])};
    auto const int32_type{
        ir.types.intern("int32", {ir.types.dependency("int32", "CoreMinimal.h")})};
    NodeListBuilder children{ir};
    children.add(ir.constexpr_member(int32_type,
                                     ir.strings.intern("num_rows"),
                                     ir.literal(std::to_string(rows.size()))),
                 2);

    for (std::size_t index{0}; index < rows.size(); ++index) {
        children.add(
            ir.constexpr_member(
                int32_type,
                ir.strings.intern(
                    context.text(
                        context.declarations.static_table_rows
                            .name[context.declarations.static_table_rows.index(rows[index])]) +
                    "_index"),
                ir.literal(std::to_string(index))),
            index + 1 < rows.size() ? 1 : 2);
    }

    children.add(ir.header_function([&] {
        auto const created_function{ir.function("num", "auto")};
        ir.set_function_body(created_function, {ir.return_stmt(ir.named("num_rows"))});
        ir.set_function_return_type(created_function, int32_type, ReturnSyntax::trailing);
        ir.set_function_noexcept(created_function);
        ir.set_function_receiver(created_function, FunctionReceiver::static_);
        ir.function_flags(created_function).set_is_constexpr(true);
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        return created_function;
    }()),
                 2);

    std::vector<std::string> column_names;
    column_names.reserve(
        context.declarations.static_tables.columns[context.declarations.static_tables.index(table)]
            .size());
    for (auto const column_id : context.declarations.items(
             context.declarations.static_tables
                 .columns[context.declarations.static_tables.index(table)])) {
        auto const column{column_id};
        column_names.push_back(
            context.text(context.declarations.static_table_columns
                             .name[context.declarations.static_table_columns.index(column)]));
    }
    children.add(column_apply_arrays_function(ir, column_names), 2)
        .add(column_apply_array_pairs_function(ir, column_names), 2);

    for (auto const group :
         context.declarations.items(context.declarations.static_tables
                                        .groups[context.declarations.static_tables.index(table)])) {
        children.add(group_getter(context, group), 2);
    }

    for (auto const column_id : context.declarations.items(
             context.declarations.static_tables
                 .columns[context.declarations.static_tables.index(table)])) {
        auto const column{column_id};
        children.add(
            ir.instance_member(
                static_array_type(
                    ir,
                    context.resolve(
                        context.declarations.static_table_columns
                            .type[context.declarations.static_table_columns.index(column)])),
                context.declarations.static_table_columns
                    .name[context.declarations.static_table_columns.index(column)],
                ir.init_list({})),
            1);
    }

    auto const created_record{ir.record(
        context.declarations.static_tables.name[context.declarations.static_tables.index(table)],
        children.build(),
        RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.static_tables
            .export_specifier[context.declarations.static_tables.index(table)]);
    return created_record;
}

} // namespace

auto lower_static_table(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {table_node(context, schema)}};
}

} // namespace lispb::codegen::detail
