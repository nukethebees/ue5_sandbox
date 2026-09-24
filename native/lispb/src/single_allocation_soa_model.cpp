#include "lowering_utils.h"
#include "single_allocation_soa_internal.h"

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
            .size_type = "std::int32_t",
            .byte_size_type = "std::size_t",
            .alignment_argument_type = "std::uint32_t",
            .span_template = "std::span",
            .span_count_requires_cast = true,
            .default_allocate_function = "ml::native_soa::allocate",
            .default_free_function = "ml::native_soa::free",
            .default_free_requires_alignment = true,
            .column_iteration_function = "each_column",
            .column_application_function = "each_column",
            .dependencies = {{"single_allocation_storage",
                              "sandbox/core/native_soa/storage.h",
                              {}}},
        };
    }
    return {
        .runtime_namespace = "ml::soa_storage::",
        .vector_namespace = "ml::soa::",
        .size_type = "int32",
        .byte_size_type = "SIZE_T",
        .alignment_argument_type = "uint32",
        .span_template = "TArrayView",
        .default_allocate_function = "ml::soa_storage::MimallocStorageAllocator::allocate",
        .default_free_function = "ml::soa_storage::MimallocStorageAllocator::free",
        .column_iteration_function = "apply_arrays",
        .column_application_function = "apply_arrays",
        .column_iteration_returns_result = true,
        .dependencies =
            {{"single_allocation_operations", "SandboxCore/single_allocation/operations.h", {}},
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
        .dialect = std::move(dialect),
        .owner_name = *schema.single_allocation,
        .layout_name = schema.name + "SingleLayout",
        .view_name = schema.name + "SingleView",
        .const_view_name = schema.name + "SingleConstView",
        .schema_const_view_name = schema.const_view_name.value_or(schema.name + "ConstView"),
        .emit_shared_types = !schema.single_allocation_allocator.has_value(),
    };
    result.dependencies = result.dialect.dependencies;

    auto allocator_dependencies =
        backend == SoaBackend::standard_library
            ? std::vector<TypeDependency>{{"single_allocation_allocator",
                                           "sandbox/core/native_soa/storage.h",
                                           {}}}
            : std::vector<TypeDependency>{
                  {"single_allocation_allocator", "SandboxCore/mimalloc_storage_allocator.h", {}}};
    result.allocate_function =
        CppType{result.dialect.default_allocate_function, allocator_dependencies};
    result.free_function = CppType{result.dialect.default_free_function, allocator_dependencies};
    result.free_requires_alignment = result.dialect.default_free_requires_alignment;

    if (schema.single_allocation_allocator) {
        auto const allocator{resolve_type(*schema.single_allocation_allocator, types)};
        result.dependencies.insert(result.dependencies.end(),
                                   allocator.dependencies.begin(),
                                   allocator.dependencies.end());
        result.allocate_function =
            CppType{allocator.spelling + "::allocate", allocator.dependencies};
        result.free_function = CppType{allocator.spelling + "::free", allocator.dependencies};
        result.free_requires_alignment = false;
    } else if (backend == SoaBackend::unreal) {
        result.dependencies.push_back({"single_allocation_mimalloc_allocator",
                                       "SandboxCore/mimalloc_storage_allocator.h",
                                       {}});
    }

    auto const root_id{type_graph.find_declared(module_name, schema.name)};
    if (!root_id.has_value()) {
        throw std::invalid_argument{"Missing resolved single-allocation SOA: " + schema.name};
    }
    auto const& root_type{std::get<lispb::schema::SoaType>(type_graph.type(*root_id).definition)};
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

    for (auto const& member : root_type.columns) {
        if (member.kind != SoaMemberKind::nested) {
            continue;
        }
        auto const& nested_type{
            std::get<lispb::schema::SoaType>(type_graph.type(*member.nested_type).definition)};
        auto const shape{recognize_compact_vector(nested_type, backend)};
        if (shape) {
            result.compact_vectors.emplace(member.name, *shape);
        }
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
