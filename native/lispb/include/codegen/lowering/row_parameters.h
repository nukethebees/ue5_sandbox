#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct RowParameters {
    auto append() -> RowParameterId;
    auto index(RowParameterId id) const -> std::uint32_t;
    void clear();
    std::vector<CppTypeId> type;
    std::vector<IdentifierId> name;
    std::vector<LoweringPathId> column;
    std::vector<std::uint8_t> nested;
};
}
