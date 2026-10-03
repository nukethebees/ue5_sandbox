#include "lowering_utils.h"
#include "single_allocation_soa_internal.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto layout_column_name(std::string_view const identifier) -> std::string {
    return title_case_identifier(identifier) + "Column";
}

auto make_dialect(SoaBackend const backend) -> SingleAllocationDialect {
    if (backend == SoaBackend::standard_library) {
        return {
            .runtime_namespace = "ml::native_soa::",
            .vector_namespace = "ml::native_soa::",
            .size_type = "std::uint32_t",
            .byte_size_type = "std::size_t",
            .span_template = "std::span",
            .span_count_requires_cast = true,
            .column_iteration_function = "each_column",
            .column_application_function = "each_column",
            .dependencies = {{"assert", "cassert", {}},
                             {"single_allocation_storage",
                              "sandbox/core/native_soa/storage.h",
                              {}}},
        };
    }
    return {
        .runtime_namespace = "ml::soa_storage::",
        .vector_namespace = "ml::soa::",
        .size_type = "int32",
        .byte_size_type = "SIZE_T",
        .span_template = "TArrayView",
        .column_iteration_function = "apply_arrays",
        .column_application_function = "apply_arrays",
        .column_iteration_returns_result = true,
        .dependencies =
            {{"assert", "cassert", {}},
             {"single_allocation_operations", "SandboxCore/single_allocation/operations.h", {}},
             {"single_allocation_removal", "sandbox/core/single_allocation/removal.h", {}},
             {"single_allocation_vector_views", "SandboxCore/single_allocation/vector_views.h", {}},
             {"single_allocation_memory_ops", "Templates/MemoryOps.h", {}}},
    };
}

auto recognize_compact_vector(lispb::schema::SoaType const& type, SoaBackend const backend)
    -> std::optional<CompactVectorShape> {
    auto const dimensions{type.vector_components.size()};
    if (dimensions == 0) {
        return std::nullopt;
    }
    auto element_type{type.columns.front().semantic_type.cpp_type.spelling};
    if (backend == SoaBackend::standard_library) {
        element_type = native_spelling(element_type);
    }
    return CompactVectorShape{std::move(element_type), dimensions};
}

auto relative_column_spelling(std::string const& spelling,
                              std::string const& namespace_name,
                              SingleAllocationModel const& model) -> std::string {
    auto const prefix{namespace_name + "::"};
    if (namespace_name.empty() || !spelling.starts_with(prefix) ||
        spelling.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789:") !=
            std::string::npos) {
        return spelling;
    }

    auto const relative{spelling.substr(prefix.size())};
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
    if (generated_names.contains(first) || first == model.owner_name ||
        first == model.layout_name || first == model.view_name || first == model.const_view_name) {
        return spelling;
    }
    for (auto const& [name, schema] : *model.schemas) {
        if (!schema->using_declarations.empty()) {
            return spelling;
        }
        for (auto const& member : schema->members) {
            if (first == member.name || first == "view_" + member.name) {
                return spelling;
            }
        }
        for (auto const* functions :
             {&schema->functions, &schema->mutable_view_functions, &schema->const_view_functions}) {
            if (std::ranges::any_of(*functions, [&](FunctionSchema const& function) {
                    return function.name == first;
                })) {
                return spelling;
            }
        }
    }
    for (auto const& column : model.columns) {
        if (first == column.flattened_identifier || first == column.layout_identifier) {
            return spelling;
        }
    }
    return relative;
}

} // namespace

auto SingleAllocationDialect::span_count(Expr count) const -> Expr {
    if (!span_count_requires_cast) {
        return count;
    }
    return static_cast_expr("std::size_t", std::move(count));
}

auto build_single_allocation_model(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   TypeRegistry const& types,
                                   lispb::schema::TypeGraph const& type_graph,
                                   std::string const& module_name,
                                   SoaBackend const backend) -> SingleAllocationModel {
    auto dialect{make_dialect(backend)};
    SingleAllocationModel result{
        .schema = &schema,
        .schemas = &schemas,
        .types = &types,
        .backend = backend,
        .dialect = std::move(dialect),
        .owner_name = *schema.single_allocation,
        .layout_name = schema.name + "SingleLayout",
        .view_name = schema.compact_view_name(),
        .const_view_name = schema.compact_const_view_name(),
    };
    result.dependencies = result.dialect.dependencies;

    result.dependencies.push_back({"memory_resource", "memory_resource", {}});

    auto const root_id{type_graph.find_declared(module_name, schema.name)};
    if (!root_id.has_value()) {
        throw std::invalid_argument{"Missing resolved single-allocation SOA: " + schema.name};
    }
    auto const& root_type{std::get<lispb::schema::SoaType>(type_graph.type(*root_id).definition)};
    result.equivalent_constructors.emplace(schema.name,
                                           root_type.equivalent_constructor.value_or(""));
    std::set<std::string> layout_names{"ColLayout", "LayoutStart", result.layout_name};
    auto collect_columns = [&](auto&& self,
                               lispb::schema::SoaType const& current,
                               std::vector<std::string> const& prefix) -> void {
        for (auto const& member : current.columns) {
            auto path{prefix};
            path.push_back(member.name);
            if (member.kind == SoaMemberKind::nested) {
                auto const& nested{std::get<lispb::schema::SoaType>(
                    type_graph.type(*member.nested_type).definition)};
                auto const& nested_schema{
                    *schemas.at(type_graph.type(*member.nested_type).identity.name)};
                result.equivalent_constructors.emplace(nested_schema.name,
                                                       nested.equivalent_constructor.value_or(""));
                if (nested_schema.uses_compact_vector_runtime()) {
                    if (auto const shape{recognize_compact_vector(nested, backend)}) {
                        result.compact_vectors.emplace(join(path, "_"), *shape);
                    }
                }
                self(self, nested, path);
                continue;
            }
            auto type{member.semantic_type.cpp_type};
            if (backend == SoaBackend::standard_library) {
                type.spelling = native_spelling(type.spelling);
            }
            auto const flattened{join(path, "_")};
            auto const layout_identifier{layout_column_name(flattened)};
            if (!layout_names.insert(layout_identifier).second) {
                throw std::invalid_argument{"Single-allocation column name collision: " +
                                            layout_identifier};
            }
            result.column_indices.emplace(flattened, result.columns.size());
            result.columns.push_back({flattened, layout_identifier, type, path});
            result.dependencies.insert(
                result.dependencies.end(), type.dependencies.begin(), type.dependencies.end());
        }
    };
    collect_columns(collect_columns, root_type, {});

    auto const& namespace_name{type_graph.type(*root_id).identity.namespace_name};
    for (auto& column : result.columns) {
        column.type.spelling =
            relative_column_spelling(column.type.spelling, namespace_name, result);
    }

    return result;
}

auto column_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> SingleAllocationColumn const& {
    return model.columns.at(model.column_indices.at(join(path, "_")));
}

auto compact_vector_for(SingleAllocationModel const& model, std::vector<std::string> const& path)
    -> CompactVectorShape const* {
    auto const found{model.compact_vectors.find(join(path, "_"))};
    return found == model.compact_vectors.end() ? nullptr : &found->second;
}

} // namespace codegen::detail
