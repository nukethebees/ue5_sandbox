#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct SingleAllocationModels {
    auto append() -> SingleAllocationModelId;
    auto index(SingleAllocationModelId id) const -> std::uint32_t;
    void clear();
    std::vector<DeclarationHandle> schema;
    std::vector<SoaBackend> backend;
    std::vector<IdentifierId> owner_name;
    std::vector<IdentifierId> layout_name;
    std::vector<IdentifierId> view_name;
    std::vector<IdentifierId> const_view_name;
    std::vector<StringId> runtime_namespace;
    std::vector<StringId> vector_namespace;
    std::vector<CppTypeId> size_type;
    std::vector<CppTypeId> byte_size_type;
    std::vector<IdentifierId> span_template;
    std::vector<IdentifierId> column_iteration_function;
    std::vector<std::uint8_t> column_iteration_returns_result;
    std::vector<SchemaRange<SingleAllocationColumnId>> columns;
    std::vector<SchemaRange<TypeDependencyId>> dependencies;
};
}
