#include "lowering_utils.h"
#include "single_allocation_soa_internal.h"

#include <cctype>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto collect_cpp_identifiers(std::string_view const spelling) -> std::set<std::string> {
    std::set<std::string> result;
    for (std::size_t start{}; start < spelling.size();) {
        if (auto const character{static_cast<unsigned char>(spelling[start])};
            !std::isalpha(character) && character != '_') {
            ++start;
            continue;
        }
        auto end{start + 1};
        while (end < spelling.size()) {
            auto const character{static_cast<unsigned char>(spelling[end])};
            if (!std::isalnum(character) && character != '_') {
                break;
            }
            ++end;
        }
        result.emplace(spelling.substr(start, end - start));
        start = end;
    }
    return result;
}

auto column_name(std::string_view const identifier) -> std::string {
    std::string result;
    result.reserve(identifier.size());
    bool capitalize{true};
    for (auto const character : identifier) {
        if (character == '_') {
            capitalize = true;
            continue;
        }
        result.push_back(
            capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                       : character);
        capitalize = false;
    }
    return result;
}

auto layout_column_name(std::string_view const identifier,
                        std::set<std::string> const& type_identifiers) -> std::string {
    auto result{column_name(identifier)};
    if (type_identifiers.contains(result)) {
        result += "Column";
    }
    return result;
}

void validate_flattened_name(std::string const& identifier) {
    if (identifier.find("__") != std::string::npos || identifier.back() == '_') {
        throw std::invalid_argument{"Single-allocation leaf would generate reserved identifiers: " +
                                    identifier};
    }
}

auto make_dialect(bool const native) -> SingleAllocationDialect {
    if (native) {
        return {
            .runtime_namespace = "ml::native_soa::",
            .vector_namespace = "ml::native_soa::",
            .size_type = "std::int32_t",
            .byte_size_type = "std::size_t",
            .alignment_argument_type = "std::uint32_t",
            .copy_function = "std::memcpy",
            .copy_header = "cstring",
            .span_template = "std::span",
            .span_count_requires_cast = true,
            .source_data_member = "data",
            .default_construct_prefix = "std::uninitialized_value_construct_n<",
            .default_construct_type_suffix = "*>",
            .default_construct_header = "memory",
            .default_allocate_function = "ml::native_soa::allocate",
            .default_free_function = "ml::native_soa::free",
            .default_free_requires_alignment = true,
            .column_iteration_function = "each_column",
            .column_application_function = "each_column",
            .dependencies = {{"single_allocation_storage", "native_soa/storage.h", {}}},
        };
    }
    return {
        .runtime_namespace = "ml::soa_storage::",
        .vector_namespace = "ml::soa::",
        .size_type = "int32",
        .byte_size_type = "SIZE_T",
        .alignment_argument_type = "uint32",
        .copy_function = "FMemory::Memcpy",
        .copy_header = "HAL/UnrealMemory.h",
        .span_template = "TArrayView",
        .source_data_member = "GetData",
        .default_construct_prefix = "DefaultConstructItems<",
        .default_construct_type_suffix = ">",
        .default_construct_header = "Templates/MemoryOps.h",
        .default_allocate_function = "ml::soa_storage::MimallocStorageAllocator::allocate",
        .default_free_function = "ml::soa_storage::MimallocStorageAllocator::free",
        .column_iteration_function = "apply_arrays",
        .column_application_function = "apply_arrays",
        .column_iteration_returns_result = true,
        .dependencies =
            {{"single_allocation_operations", "SandboxCore/single_allocation/operations.h", {}},
             {"single_allocation_removal", "SandboxCore/single_allocation/removal.h", {}},
             {"single_allocation_vector_views", "SandboxCore/single_allocation/vector_views.h", {}},
             {"single_allocation_memory_ops", "Templates/MemoryOps.h", {}}},
    };
}

auto recognize_compact_vector(SoaSchema const& schema,
                              std::string const& prefix,
                              std::map<std::string, SingleAllocationColumn const*> const& columns)
    -> std::optional<CompactVectorShape> {
    auto const dimensions{schema.members.size()};
    if (dimensions != 2 && dimensions != 3) {
        return std::nullopt;
    }
    std::string type;
    for (std::size_t index{}; index < dimensions; ++index) {
        auto const& member{schema.members[index]};
        if (member.kind != SoaMemberKind::array ||
            member.name != std::string(1, "xyz"[index]) + "s") {
            return std::nullopt;
        }
        auto const& element{columns.at(prefix + "_" + member.name)->type.spelling};
        if (index == 0) {
            type = element;
        } else if (element != type) {
            return std::nullopt;
        }
    }
    static std::set<std::string> const scalars{"float",
                                               "double",
                                               "int8",
                                               "uint8",
                                               "int16",
                                               "uint16",
                                               "int32",
                                               "uint32",
                                               "int64",
                                               "uint64",
                                               "std::int8_t",
                                               "std::uint8_t",
                                               "std::int16_t",
                                               "std::uint16_t",
                                               "std::int32_t",
                                               "std::uint32_t",
                                               "std::int64_t",
                                               "std::uint64_t"};
    if (!scalars.contains(type)) {
        return std::nullopt;
    }
    return CompactVectorShape{type, dimensions};
}

} // namespace

auto SingleAllocationDialect::span_type(std::string const& element_type, bool const is_const) const
    -> std::string {
    return span_template + "<" + element_type + (is_const ? " const" : "") + ">";
}

auto SingleAllocationDialect::span_count(Expr count) const -> Expr {
    if (!span_count_requires_cast) {
        return count;
    }
    return static_cast_expr("std::size_t", std::move(count));
}

auto build_single_allocation_model(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   std::map<std::string, CppType> const& types,
                                   bool const native) -> SingleAllocationModel {
    auto const layout{build_soa_layout(schema, schemas, types, false)};
    auto dialect{make_dialect(native)};
    SingleAllocationModel result{
        .schema = &schema,
        .schemas = &schemas,
        .dialect = std::move(dialect),
        .owner_name = *schema.single_allocation,
        .storage_name = *schema.single_allocation + "Storage",
        .layout_name = schema.name + "SingleLayout",
        .view_name = schema.name + "SingleView",
        .const_view_name = schema.name + "SingleConstView",
        .emit_shared_types = !schema.single_allocation_allocator.has_value(),
    };
    result.dependencies = result.dialect.dependencies;

    auto allocator_dependencies =
        native
            ? std::vector<TypeDependency>{{"single_allocation_allocator",
                                           "native_soa/storage.h",
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
    } else if (!native) {
        result.dependencies.push_back({"single_allocation_mimalloc_allocator",
                                       "SandboxCore/mimalloc_storage_allocator.h",
                                       {}});
    }

    std::set<std::string> type_identifiers;
    for (auto const& leaf : layout.leaves) {
        auto const identifiers{collect_cpp_identifiers(leaf.type.spelling)};
        type_identifiers.insert(identifiers.begin(), identifiers.end());
    }

    std::set<std::string> flattened_names;
    std::set<std::string> layout_names{"ColLayout", "LayoutStart", result.layout_name};
    std::map<std::string, std::size_t> unique_type_indices;
    result.columns.reserve(layout.leaves.size());
    for (auto const& leaf : layout.leaves) {
        auto const flattened{fixed_leaf_argument(leaf)};
        validate_flattened_name(flattened);
        if (!flattened_names.insert(flattened).second) {
            throw std::invalid_argument{"Single-allocation flattened leaf name collision: " +
                                        flattened};
        }
        auto const layout_identifier{layout_column_name(flattened, type_identifiers)};
        if (!layout_names.insert(layout_identifier).second) {
            throw std::invalid_argument{"Single-allocation column name collision: " +
                                        layout_identifier};
        }

        auto [type_position, inserted]{
            unique_type_indices.emplace(leaf.type.spelling, result.unique_types.size())};
        if (inserted) {
            result.unique_types.push_back({leaf.type, leaf.path, flattened});
        }
        auto const& byte_count_identifier{
            result.unique_types[type_position->second].byte_count_identifier};
        result.column_indices.emplace(flattened, result.columns.size());
        result.columns.push_back(
            {flattened, layout_identifier, leaf.type, leaf.path, byte_count_identifier});
        result.dependencies.insert(result.dependencies.end(),
                                   leaf.type.dependencies.begin(),
                                   leaf.type.dependencies.end());
    }

    std::map<std::string, SingleAllocationColumn const*> columns;
    for (auto const& column : result.columns) {
        columns.emplace(column.flattened_identifier, &column);
    }
    for (auto const& member : schema.members) {
        if (member.kind != SoaMemberKind::nested) {
            continue;
        }
        auto const shape{
            recognize_compact_vector(*schemas.at(*member.nested_schema), member.name, columns)};
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
