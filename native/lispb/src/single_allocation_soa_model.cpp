#include "single_allocation_soa_model.h"

#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/soa_backend.h>
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

auto recognize_compact_vector(LoweringContext& context,
                              schema::TypeDefinitionId type,
                              SoaBackend backend) -> CompactVectorId {
    auto& ir{context.ir};
    auto const& definitions{context.semantic.definitions()};
    auto const components{definitions.soas.vector_components_of(type)};
    if (components.empty()) {
        return {};
    }
    auto const occurrence{definitions.soa_columns.semantic_type[definitions.soa_columns.index(
        definitions.soas.columns_of(type).front())]};
    auto const& occurrences{context.semantic.occurrences()};
    auto element{occurrences.cpp_type[occurrences.index(occurrence)]};
    if (backend == SoaBackend::standard_library) {
        element =
            ir.types.derive(element, native_spelling(std::string{ir.types.spelling(element)}));
    }
    auto& vectors{ir.lowering.compact_vectors};
    auto const result{vectors.append()};
    auto const row{vectors.index(result)};
    vectors.element_type[row] = element;
    vectors.components[row] = ir.lowering.save(components);
    vectors.kind[row] = ir.strings.view(components.front()) == "pitches"
                          ? CompactVectorKind::rotator
                      : components.size() == 2 ? CompactVectorKind::vector2
                                               : CompactVectorKind::vector3;
    return result;
}

auto relative_column_spelling(LoweringContext& context,
                              std::string const& spelling,
                              std::string const& namespace_name,
                              SingleAllocationModelId model,
                              std::map<IdentifierId, DeclarationHandle> const& schemas)
    -> std::string {
    auto& model_store{context.ir.lowering.single_allocation_models};
    auto const model_store_index{model_store.index(model)};
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto const prefix{namespace_name + "::"};
    if (namespace_name.empty() || !spelling.starts_with(prefix) ||
        spelling.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789:") !=
            std::string::npos) {
        return spelling;
    }

    auto relative{spelling.substr(prefix.size())};
    auto const first{relative.substr(0, relative.find("::"))};
    // Preserve qualification when class or template scope hides the namespace member.
    static std::set<std::string_view> const generated_names{"Base",
                                                            "Byte",
                                                            "ColLayout",
                                                            "Columns",
                                                            "Const",
                                                            "ConstView",
                                                            "DataPointers",
                                                            "Element",
                                                            "Enabled",
                                                            "Func",
                                                            "Layout",
                                                            "LayoutStart",
                                                            "Operations",
                                                            "Overlapping",
                                                            "Self",
                                                            "State",
                                                            "Storage",
                                                            "T",
                                                            "View",
                                                            "size_type",
                                                            "byte_size_type",
                                                            "equivalent_type",
                                                            "soa_schema",
                                                            "allocation_alignment",
                                                            "capacity_granularity",
                                                            "column_gap",
                                                            "capacity_block_bound",
                                                            "max_capacity",
                                                            "layout_bytes",
                                                            "data_",
                                                            "num_",
                                                            "capacity_",
                                                            "resource_",
                                                            "state_",
                                                            "count_",
                                                            "offset_",
                                                            "source",
                                                            "count",
                                                            "first",
                                                            "source_first",
                                                            "blocks",
                                                            "capacity_blocks",
                                                            "validate",
                                                            "num",
                                                            "capacity",
                                                            "is_empty",
                                                            "allocated_bytes",
                                                            "get_memory_resource",
                                                            "get_view",
                                                            "get_const_view",
                                                            "slice",
                                                            "left",
                                                            "right",
                                                            "get_data",
                                                            "make_data_unchecked",
                                                            "default_construct_columns",
                                                            "swap_remove_columns",
                                                            "swap_remove_indices",
                                                            "copy_columns",
                                                            "transfer_columns",
                                                            "source_pointers",
                                                            "copy_columns_from",
                                                            "append_columns",
                                                            "copy_live_columns",
                                                            "each_column",
                                                            "apply_arrays",
                                                            "add_defaulted",
                                                            "add_uninitialised",
                                                            "append_from",
                                                            "copy_element",
                                                            "copy_elements",
                                                            "remove_at_swap",
                                                            "reserve",
                                                            "reset",
                                                            "set_num",
                                                            "reallocate"};
    if (generated_names.contains(first) ||
        first == context.text(model_store.owner_name[model_store_index]) ||
        first == context.text(model_store.layout_name[model_store_index]) ||
        first == context.text(model_store.view_name[model_store_index]) ||
        first == context.text(model_store.const_view_name[model_store_index])) {
        return spelling;
    }
    for (auto const& [name, schema] : schemas) {
        if (!context.declarations.soas.using_declarations[context.declarations.soas.index(schema)]
                 .empty()) {
            return spelling;
        }
        for (auto const member_id : context.declarations.items(
                 context.declarations.soas.members[context.declarations.soas.index(schema)])) {
            auto const member{member_id};
            if (first == context.text(context.declarations.soa_members
                                          .name[context.declarations.soa_members.index(member)]) ||
                first == "view_" + context.text(
                                       context.declarations.soa_members
                                           .name[context.declarations.soa_members.index(member)])) {
                return spelling;
            }
        }
        for (auto const* functions :
             {&context.declarations.soas.functions[context.declarations.soas.index(schema)],
              &context.declarations.soas
                   .mutable_view_functions[context.declarations.soas.index(schema)],
              &context.declarations.soas
                   .const_view_functions[context.declarations.soas.index(schema)]}) {
            if (std::ranges::any_of(
                    context.declarations.items(*functions), [&](auto const function_id) {
                        return context.text(
                                   context.declarations.functions
                                       .name[context.declarations.functions.index(function_id)]) ==
                               first;
                    })) {
                return spelling;
            }
        }
    }
    for (auto const column : context.ir.lowering.items(model_store.columns[model_store_index])) {
        if (first == context.text(column_store.flattened_identifier[column_store.index(column)]) ||
            first == context.text(column_store.layout_identifier[column_store.index(column)])) {
            return spelling;
        }
    }
    return relative;
}

} // namespace

auto build_single_allocation_model(LoweringContext& context,
                                   DeclarationHandle schema,
                                   std::map<IdentifierId, DeclarationHandle> const& schemas,
                                   SoaBackend const backend) -> SingleAllocationModelId {
    auto& column_store{context.ir.lowering.single_allocation_columns};
    auto& ir{context.ir};
    auto const& type_graph{context.semantic};
    auto const module_name{context.module};
    auto& storage{ir.lowering};
    auto& models{storage.single_allocation_models};
    auto& columns{storage.single_allocation_columns};
    auto& vectors{storage.compact_vectors};
    auto const result{models.append()};
    auto const row{models.index(result)};
    auto const& sources{context.declarations.soas};
    auto const source_row{sources.index(schema)};
    models.schema[row] = schema;
    models.backend[row] = backend;
    models.owner_name[row] = *sources.single_allocation[source_row];
    models.layout_name[row] =
        ir.strings.intern(context.text(sources.name[source_row]) + "SingleLayout");
    models.view_name[row] =
        sources.emits_vector_storage(schema) || !sources.view_name[source_row]
            ? ir.strings.intern(context.text(sources.name[source_row]) + "SingleView")
            : *sources.view_name[source_row];
    models.const_view_name[row] =
        sources.emits_vector_storage(schema) || !sources.const_view_name[source_row]
            ? ir.strings.intern(context.text(sources.name[source_row]) + "SingleConstView")
            : *sources.const_view_name[source_row];
    auto const native{backend == SoaBackend::standard_library};
    models.runtime_namespace[row] =
        ir.strings.add(native ? "ioj::native_soa::" : "ioj::soa_storage::");
    models.vector_namespace[row] = ir.strings.add(native ? "ioj::native_soa::" : "ioj::soa::");
    models.size_type[row] = ir.types.intern(native ? "std::uint32_t" : "int32");
    models.byte_size_type[row] = ir.types.intern(native ? "std::size_t" : "SIZE_T");
    models.span_template[row] = ir.strings.intern(native ? "std::span" : "TArrayView");
    models.column_iteration_function[row] =
        ir.strings.intern(native ? "each_column" : "apply_arrays");
    models.column_iteration_returns_result[row] = !native;
    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("assert", "cassert"),
        ir.types.dependency("memory_resource", "memory_resource")};
    if (native) {
        dependencies.push_back(
            ir.types.dependency("single_allocation_storage", "ioj/native_soa/storage.h"));
    } else {
        dependencies.push_back(ir.types.dependency("single_allocation_operations",
                                                   "SandboxCore/single_allocation/operations.h"));
        dependencies.push_back(
            ir.types.dependency("single_allocation_removal", "ioj/single_allocation/removal.h"));
        dependencies.push_back(ir.types.dependency("single_allocation_vector_views",
                                                   "SandboxCore/single_allocation/vector_views.h"));
        dependencies.push_back(
            ir.types.dependency("single_allocation_memory_ops", "Templates/MemoryOps.h"));
    }
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
                auto const nested{type_graph.definition(
                    *type_graph.definitions()
                         .soa_columns
                         .nested_type[type_graph.definitions().soa_columns.index(member)])};
                auto const& nested_schema{schemas.at(type_graph.name(
                    *type_graph.definitions()
                         .soa_columns
                         .nested_type[type_graph.definitions().soa_columns.index(member)]))};
                storage.equivalent_constructors.emplace(
                    std::pair{result, nested_schema},
                    type_graph.definitions()
                        .soas.equivalent_constructor[type_graph.definitions().soas.index(nested)]
                        .value_or(StringId{}));
                if (auto shape{recognize_compact_vector(context, nested, backend)};
                    shape.value != 0) {
                    auto use_runtime{
                        context.declarations.soas.uses_compact_vector_runtime(nested_schema)};
                    if (backend == SoaBackend::standard_library &&
                        context.declarations.soas
                            .layout_only[context.declarations.soas.index(nested_schema)] &&
                        context.declarations.soas
                            .equivalent_type[context.declarations.soas.index(nested_schema)] &&
                        context.declarations.soas
                            .const_view_functions[context.declarations.soas.index(nested_schema)]
                            .empty() &&
                        context.declarations.soas
                            .mutable_view_functions[context.declarations.soas.index(nested_schema)]
                            .empty() &&
                        context.declarations.soas
                            .using_declarations[context.declarations.soas.index(nested_schema)]
                            .empty() &&
                        context.declarations.soas
                                .export_specifier[context.declarations.soas.index(nested_schema)]
                                .value == 0) {
                        if (vectors.kind[vectors.index(shape)] == CompactVectorKind::vector3) {
                            use_runtime = uses_native_vector_view(context, nested_schema);
                        } else if (vectors.kind[vectors.index(shape)] ==
                                       CompactVectorKind::rotator &&
                                   ir.strings
                                       .view(type_graph.definitions()
                                                 .soas
                                                 .equivalent_constructor[type_graph.definitions()
                                                                             .soas.index(nested)]
                                                 .value_or(StringId{}))
                                       .empty()) {
                            auto const equivalent{context.resolve(
                                *context.declarations.soas
                                     .equivalent_type[context.declarations.soas.index(
                                         nested_schema)])};
                            vectors.equivalent_type[vectors.index(shape)] = equivalent;
                            dependencies.insert(dependencies.end(),
                                                ir.types.dependencies(equivalent).begin(),
                                                ir.types.dependencies(equivalent).end());
                            use_runtime = true;
                        }
                    }
                    if (use_runtime) {
                        storage.compact_vector_lookup.emplace(std::pair{result, path}, shape);
                    }
                }
                self(self, nested, path);
                continue;
            }
            auto type{type_graph.occurrences().cpp_type[type_graph.occurrences().index(
                type_graph.definitions().soa_columns.semantic_type
                    [type_graph.definitions().soa_columns.index(member)])]};
            if (backend == SoaBackend::standard_library) {
                type = ir.types.derive(type, native_spelling(std::string{ir.types.spelling(type)}));
            }
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

    auto const& namespace_name{type_graph.namespace_name(*root_id)};
    models.columns[row] = storage.save(std::span<SingleAllocationColumnId const>{column_ids});
    for (auto const column : column_ids) {
        column_store.type[column_store.index(column)] = ir.types.derive(
            column_store.type[column_store.index(column)],
            relative_column_spelling(
                context,
                std::string{ir.types.spelling(column_store.type[column_store.index(column)])},
                context.text(namespace_name),
                result,
                schemas));
    }

    models.dependencies[row] = storage.save(std::span<TypeDependencyId const>{dependencies});
    return result;
}

auto column_for(LoweringContext& context, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId {
    return context.ir.lowering.column_lookup.at({model, path});
}

auto compact_vector_for(LoweringContext& context,
                        SingleAllocationModelId model,
                        LoweringPathId path) -> CompactVectorId {
    auto const& lookup{context.ir.lowering.compact_vector_lookup};
    auto const found{lookup.find({model, path})};
    return found == lookup.end() ? CompactVectorId{} : found->second;
}

} // namespace lispb::codegen::detail
