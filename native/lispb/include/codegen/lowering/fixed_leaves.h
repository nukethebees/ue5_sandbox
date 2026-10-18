#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct FixedLeaves {
    auto append() -> FixedLeafId;
    auto index(FixedLeafId id) const -> std::uint32_t;
    void clear();
    std::vector<LoweringPathId> path;
    std::vector<CppTypeId> type;
};
}
