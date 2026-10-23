#include "single_allocation_soa_model.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declaration_schema.h>
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
auto schema_namespace(LoweringContext& context, DeclarationHandle schema) -> std::string {
    auto const scoped{context.for_declaration(schema)};
    auto const& declarations{context.declarations};
    auto const& settings{declarations.module_settings_store};
    auto const namespace_name{
        settings.namespace_name[settings.index(declarations.module_settings(scoped.module))]};
    return (namespace_name ? "::" + context.text(*namespace_name) + "::" : "::") +
           soa_namespace_name(
               context.text(declarations.soas.name[declarations.soas.index(schema)]));
}

auto is_strided_schema(LoweringContext& context, DeclarationHandle schema) -> bool {
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const& members{declarations.soa_members};
    std::optional<CppTypeId> element_type;
    std::size_t count{};
    auto homogeneous = [&](auto&& self, DeclarationHandle current) -> bool {
        auto const scoped{context.for_declaration(current)};
        for (auto const member : declarations.items(soas.members[soas.index(current)])) {
            auto const row{members.index(member)};
            if (members.kind[row] == SoaMemberKind::nested) {
                if (!self(self, scoped.nested_declaration(member))) {
                    return false;
                }
            } else {
                auto const type{scoped.resolve(members.type[row])};
                if (element_type && *element_type != type) {
                    return false;
                }
                element_type = type;
                ++count;
            }
        }
        return true;
    };
    return homogeneous(homogeneous, schema) && count >= 2;
}

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
    auto const namespace_name{schema_namespace(context, schema)};
    models.layout_name[row] = ir.strings.intern(namespace_name + "::Layout");
    models.view_name[row] = ir.strings.intern(namespace_name + "::View");
    models.const_view_name[row] = ir.strings.intern(namespace_name + "::ConstView");
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
    std::set<std::string> flattened_names;
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
                auto const cpp_type{
                    type_graph.occurrences().cpp_type[type_graph.occurrences().index(occurrence)]};
                auto const nested_dependencies{ir.types.dependencies(cpp_type)};
                dependencies.insert(
                    dependencies.end(), nested_dependencies.begin(), nested_dependencies.end());
                self(self, nested, path);
                continue;
            }
            auto type{type_graph.occurrences().cpp_type[type_graph.occurrences().index(
                type_graph.definitions().soa_columns.semantic_type
                    [type_graph.definitions().soa_columns.index(member)])]};
            auto const flattened{context.path_text(path, "_")};
            if (!flattened_names.insert(flattened).second) {
                throw std::invalid_argument{"Single-allocation column name collision: " +
                                            flattened};
            }
            auto const column{columns.append()};
            auto const column_row{columns.index(column)};
            columns.flattened_identifier[column_row] = ir.strings.intern(flattened);
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
