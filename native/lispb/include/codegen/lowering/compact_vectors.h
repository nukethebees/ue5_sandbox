#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/compact_vector_kind.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct CompactVectors {
    auto append() -> CompactVectorId;
    auto index(CompactVectorId id) const -> std::uint32_t;
    void clear();
    std::vector<CppTypeId> element_type;
    std::vector<SchemaRange<IdentifierId>> components;
    std::vector<CompactVectorKind> kind;
    std::vector<CppTypeId> equivalent_type;
};
}
