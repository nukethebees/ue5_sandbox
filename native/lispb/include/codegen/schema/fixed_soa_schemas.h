#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct FixedSoaSchemas {
    [[nodiscard]] auto append() -> FixedSoaId;
    [[nodiscard]] auto clone(FixedSoaId id) -> FixedSoaId;
    [[nodiscard]] auto index(FixedSoaId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> storage_name;
    std::vector<SchemaRange<StringId>> containers;
};

}
