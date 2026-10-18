#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_storage.h>
#include <codegen/schema/storage_operation.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct SoaSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();
    auto selected_storage(DeclarationHandle id) const -> SoaStorage;
    auto emits_vector_storage(DeclarationHandle id) const -> bool;
    auto uses_compact_vector_runtime(DeclarationHandle id) const -> bool;

    std::vector<IdentifierId> name;
    std::vector<std::optional<IdentifierId>> view_name;
    std::vector<std::optional<IdentifierId>> const_view_name;
    std::vector<SchemaRange<SoaMemberId>> members;
    std::vector<SchemaRange<StorageOperation>> operations;
    std::vector<IdentifierId> export_specifier;
    std::vector<SchemaRange<FunctionSchemaId>> functions;
    std::vector<SchemaRange<FunctionSchemaId>> mutable_view_functions;
    std::vector<SchemaRange<StringId>> using_declarations;
    std::vector<std::optional<TypeRefId>> equivalent_type;
    std::vector<std::uint8_t> copy_element_memberwise;
    std::vector<std::uint8_t> layout_only;
    std::vector<std::optional<FixedSoaId>> fixed;
    std::vector<std::optional<IdentifierId>> single_allocation;
    std::vector<std::optional<TypeRefId>> array_allocator;
    std::vector<std::optional<IdentifierId>> field_mask_name;
    std::vector<std::optional<IdentifierId>> field_enum_name;
    std::vector<SchemaRange<IdentifierId>> vector_components;
    std::vector<std::optional<SoaStorage>> storage;
    std::vector<SchemaRange<FunctionSchemaId>> const_view_functions;
};

}
