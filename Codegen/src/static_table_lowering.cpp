#include "lowering.h"
#include "lowering_utils.h"

#include <utility>

namespace codegen::detail {
namespace {

auto static_array_type(CppType const& element_type) -> CppType {
    TypeDependency dependency{"TStaticArray", "Containers/StaticArray.h", element_type.dependencies};
    return CppType{
        "TStaticArray<" + element_type.spelling + ", num_rows>",
        std::vector<TypeDependency>{std::move(dependency)},
    };
}

auto table_node(StaticTableSchema const& table,
                std::map<std::string, CppType> const& types) -> Node {
    CppType const int32_type{"int32", "CoreMinimal.h"};
    NodeListBuilder children;
    children.add(Member{int32_type,
                        "num_rows",
                        std::to_string(table.rows.size()),
                        {.is_static = true, .is_constexpr = true}},
                 2);

    for (std::size_t index{0}; index < table.rows.size(); ++index) {
        children.add(Member{int32_type,
                            table.rows[index].name + "_index",
                            std::to_string(index),
                            {.is_static = true, .is_constexpr = true}},
                     index + 1 < table.rows.size() ? 1 : 2);
    }

    children.add(header_function(FunctionSpec{
                     .name = "num",
                     .return_type = "auto",
                     .body = {ReturnStatement{"num_rows"}},
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

    for (auto const& column : table.columns) {
        children.add(Member{static_array_type(resolve_type(column.type, types)), column.name, ""},
                     1);
    }

    return Struct{
        .name = table.name,
        .children = children.build(),
        .export_specifier = table.export_specifier,
    };
}

} // namespace

auto lower_static_table_module(StaticTableModuleSchema const& module,
                               std::map<std::string, CppType> const& types) -> Module {
    NodeListBuilder definitions;
    for (std::size_t index{0}; index < module.tables.size(); ++index) {
        definitions.add(table_node(module.tables[index], types),
                        index + 1 < module.tables.size() ? 2 : 1);
    }

    auto definition_nodes{definitions.build()};
    if (module.settings.namespace_name.has_value()) {
        definition_nodes = {Namespace{*module.settings.namespace_name, std::move(definition_nodes)}};
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.append(std::move(definition_nodes));

    return Module{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
}

} // namespace codegen::detail
