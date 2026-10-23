#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct SingleAllocationColumns {
    auto append() -> SingleAllocationColumnId;
    auto index(SingleAllocationColumnId id) const -> std::uint32_t;
    void clear();
    std::vector<StringId> flattened_identifier;
    std::vector<CppTypeId> type;
    std::vector<LoweringPathId> member_path;
};
}
