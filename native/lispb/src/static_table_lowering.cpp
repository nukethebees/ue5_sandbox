#include "lowering.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

auto static_array_type(CppType const& element_type) -> CppType {
    TypeDependency dependency{
        "TStaticArray", "Containers/StaticArray.h", element_type.dependencies};
    return CppType{
        "TStaticArray<" + element_type.spelling + ", num_rows>",
        std::vector<TypeDependency>{std::move(dependency)},
    };
}

auto group_getter(StaticTableGroupSchema const& group, std::map<std::string, CppType> const& types)
    -> Node {
    auto const result_type{resolve_type(group.type, types)};
    std::vector<std::string> arguments;
    arguments.reserve(group.columns.size());
    for (auto const& column : group.columns) {
        arguments.push_back(column + "[index]");
    }

    return header_function(FunctionSpec{
        .name = "get_" + group.name,
        .return_type = "auto",
        .parameters = {FunctionParameter{CppType{"int32 const", "CoreMinimal.h"}, "index"}},
        .body = {ReturnStmt{RawExpr{result_type.spelling + "{" + join(arguments, ", ") + "}"}}},
        .qualifiers = {.trailing_return_type = result_type, .is_const = true},
        .is_inline = true,
    });
}

auto table_node(StaticTableSchema const& table, std::map<std::string, CppType> const& types)
    -> Node {
    CppType const int32_type{"int32", "CoreMinimal.h"};
    NodeListBuilder children;
    children.add(Member{int32_type,
                        "num_rows",
                        RawExpr{std::to_string(table.rows.size())},
                        {.is_static = true, .is_constexpr = true}},
                 2);

    for (std::size_t index{0}; index < table.rows.size(); ++index) {
        children.add(Member{int32_type,
                            table.rows[index].name + "_index",
                            RawExpr{std::to_string(index)},
                            {.is_static = true, .is_constexpr = true}},
                     index + 1 < table.rows.size() ? 1 : 2);
    }

    children.add(header_function(FunctionSpec{
                     .name = "num",
                     .return_type = "auto",
                     .body = {ReturnStmt{RawExpr{"num_rows"}}},
                     .qualifiers =
                         {
                             .trailing_return_type = int32_type,
                             .is_noexcept = true,
                         },
                     .is_static = true,
                     .is_constexpr = true,
                     .is_inline = true,
                 }),
                 2);

    std::vector<std::string> column_names;
    column_names.reserve(table.columns.size());
    for (auto const& column : table.columns) {
        column_names.push_back(column.name);
    }
    children.add(column_apply_arrays_function(column_names), 2)
        .add(column_apply_array_pairs_function(column_names), 2);

    for (std::size_t index{0}; index < table.groups.size(); ++index) {
        children.add(group_getter(table.groups[index], types), 2);
    }

    for (auto const& column : table.columns) {
        children.add(
            Member{static_array_type(resolve_type(column.type, types)), column.name, RawExpr{""}},
            1);
    }

    return Struct{
        .name = table.name,
        .children = children.build(),
        .export_specifier = table.export_specifier,
    };
}

} // namespace

auto lower_static_table(StaticTableSchema const& schema,
                        std::map<std::string, CppType> const& types) -> DeclarationEmission {
    return {.header = {table_node(schema, types)}};
}

} // namespace codegen::detail
