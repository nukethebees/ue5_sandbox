#include "single_allocation_soa_model.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto layout_column_name(std::string_view const identifier) -> std::string {
    return title_case_identifier(identifier) + "Column";
}

} // namespace

auto build_single_allocation_model(LoweringContext& context, DeclarationHandle schema)
    -> SingleAllocationModelId {
    auto& ir{context.ir};
    auto const& type_graph{context.semantic};
    auto const module_name{context.module};
    auto& storage{ir.lowering};
    auto& models{storage.single_allocation_models};
    auto& columns{storage.single_allocation_columns};
    auto const result{models.append()};
    auto const row{models.index(result)};
    auto const& sources{context.declarations.soas};
    auto const source_row{sources.index(schema)};
    models.schema[row] = schema;
    models.owner_name[row] = sources.name[source_row];
    auto const name{context.text(sources.name[source_row])};
    models.layout_name[row] = ir.strings.intern(name + "Layout");
    models.view_name[row] =
        sources.view_name[source_row].value_or(ir.strings.intern(name + "View"));
    models.const_view_name[row] =
        sources.const_view_name[source_row].value_or(ir.strings.intern(name + "ConstView"));
    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("assert", "cassert"),
        ir.types.dependency("memory_resource", "memory_resource"),
        ir.types.dependency("apply_permutation", "ioj/soa_permutation.h"),
        ir.types.dependency("single_allocation_storage", "ioj/native_soa/storage.h")};
    std::vector<SingleAllocationColumnId> column_ids;

    auto const root_id{type_graph.find_declared(
        module_name, context.declarations.soas.name[context.declarations.soas.index(schema)])};
    if (!root_id.has_value()) {
        throw std::invalid_argument{
            "Missing resolved single-allocation SOA: " +
            context.text(context.declarations.soas.name[context.declarations.soas.index(schema)])};
    }
    auto const root_type{type_graph.definition(*root_id)};
    storage.equivalent_constructors.emplace(
        std::pair{result, schema},
        type_graph.definitions()
            .soas.equivalent_constructor[type_graph.definitions().soas.index(root_type)]
            .value_or(StringId{}));
    std::set<std::string> layout_names{"ColLayout",
                                       context.text(models.layout_name[row]) + "Columns"};
    auto collect_columns =
        [&](auto&& self, schema::TypeDefinitionId const& current, LoweringPathId prefix) -> void {
        for (auto const member_id : type_graph.definitions().soas.columns_of(current)) {
            auto const member{member_id};
            auto const path{storage.paths.append(
                prefix,
                type_graph.definitions()
                    .soa_columns.name[type_graph.definitions().soa_columns.index(member)])};
            if (type_graph.definitions()
                    .soa_columns.kind[type_graph.definitions().soa_columns.index(member)] ==
                SoaMemberKind::nested) {
                auto const occurrence{
                    type_graph.definitions()
                        .soa_columns
                        .semantic_type[type_graph.definitions().soa_columns.index(member)]};
                auto const nested_type{
                    type_graph.occurrences().type[type_graph.occurrences().index(occurrence)]};
                auto const nested{type_graph.definition(nested_type)};
                auto const nested_schema{type_graph.source_declaration(nested_type)};
                storage.equivalent_constructors.emplace(
                    std::pair{result, nested_schema},
                    type_graph.definitions()
                        .soas.equivalent_constructor[type_graph.definitions().soas.index(nested)]
                        .value_or(StringId{}));
                if (!context.declarations.soas
                         .layout_only[context.declarations.soas.index(nested_schema)]) {
                    auto const cpp_type{type_graph.occurrences()
                                            .cpp_type[type_graph.occurrences().index(occurrence)]};
                    auto const nested_dependencies{ir.types.dependencies(cpp_type)};
                    dependencies.insert(
                        dependencies.end(), nested_dependencies.begin(), nested_dependencies.end());
                }
                self(self, nested, path);
                continue;
            }
            auto type{type_graph.occurrences().cpp_type[type_graph.occurrences().index(
                type_graph.definitions().soa_columns.semantic_type
                    [type_graph.definitions().soa_columns.index(member)])]};
            auto const flattened{context.path_text(path, "_")};
            auto const layout_identifier{layout_column_name(flattened)};
            if (!layout_names.insert(layout_identifier).second) {
                throw std::invalid_argument{"Single-allocation column name collision: " +
                                            layout_identifier};
            }
            auto const column{columns.append()};
            auto const column_row{columns.index(column)};
            columns.flattened_identifier[column_row] = ir.strings.intern(flattened);
            columns.layout_identifier[column_row] = ir.strings.intern(layout_identifier);
            columns.type[column_row] = type;
            columns.member_path[column_row] = path;
            storage.column_lookup.emplace(std::pair{result, path}, column);
            column_ids.push_back(column);
            dependencies.insert(dependencies.end(),
                                ir.types.dependencies(type).begin(),
                                ir.types.dependencies(type).end());
        }
    };
    collect_columns(collect_columns, root_type, {});

    models.columns[row] = storage.save(std::span<SingleAllocationColumnId const>{column_ids});

    models.dependencies[row] = storage.save(std::span<TypeDependencyId const>{dependencies});
    return result;
}

auto column_for(LoweringContext& context, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId {
    return context.ir.lowering.column_lookup.at({model, path});
}

} // namespace lispb::codegen::detail
