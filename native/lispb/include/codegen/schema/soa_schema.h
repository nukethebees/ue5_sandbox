#pragma once

#include <codegen/schema/fixed_soa_schema.h>
#include <codegen/schema/function_schema.h>
#include <codegen/schema/soa_member_schema.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_ref.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

enum class SoaStorage { vector, single_allocation, both };

constexpr auto soa_storage_name(SoaStorage const storage) -> char const* {
    switch (storage) {
        case SoaStorage::vector: {
            return "vector";
        }
        case SoaStorage::single_allocation: {
            return "single-allocation";
        }
        case SoaStorage::both: {
            return "both";
        }
    }
    return "";
}

struct SingleAllocationVariant {
    std::string name;
    TypeRef allocator;
};

struct SoaSchema {
    std::string name;
    std::optional<std::string> view_name;
    std::optional<std::string> const_view_name;
    std::vector<SoaMemberSchema> members;
    std::vector<StorageOperation> operations;
    std::optional<std::string> export_specifier;
    std::vector<FunctionSchema> functions;
    std::vector<FunctionSchema> mutable_view_functions;
    std::vector<std::string> using_declarations;
    std::optional<TypeRef> equivalent_type;
    bool copy_element_memberwise{false};
    bool layout_only{false};
    std::optional<FixedSoaSchema> fixed;
    std::optional<std::string> single_allocation;
    std::optional<TypeRef> array_allocator;
    std::vector<SingleAllocationVariant> single_allocation_variants;
    std::optional<TypeRef> single_allocation_allocator;
    std::optional<std::string> field_mask_name;
    std::optional<std::string> field_enum_name;
    std::vector<std::string> vector_components;
    std::optional<SoaStorage> storage;
    std::vector<FunctionSchema> const_view_functions;

    auto uses_compact_vector_runtime() const -> bool {
        return !vector_components.empty() && !equivalent_type && const_view_functions.empty() &&
               mutable_view_functions.empty() && using_declarations.empty() && !export_specifier;
    }

    auto selected_storage() const -> SoaStorage {
        return storage.value_or(single_allocation ? SoaStorage::single_allocation
                                                  : SoaStorage::vector);
    }
    auto has_operation(StorageOperation operation) const -> bool {
        return std::ranges::find(operations, operation) != operations.end();
    }
    auto compact_view_name() const -> std::string {
        return emits_vector_storage() ? name + "SingleView"
                                      : view_name.value_or(name + "SingleView");
    }
    auto compact_const_view_name() const -> std::string {
        return emits_vector_storage() ? name + "SingleConstView"
                                      : const_view_name.value_or(name + "SingleConstView");
    }

    auto emits_vector_storage() const -> bool {
        return !layout_only && selected_storage() != SoaStorage::single_allocation;
    }
};

} // namespace codegen
