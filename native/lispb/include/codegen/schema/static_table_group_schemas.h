#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct StaticTableGroupSchemas {
    [[nodiscard]] auto append() -> StaticTableGroupId;
    [[nodiscard]] auto clone(StaticTableGroupId id) -> StaticTableGroupId;
    [[nodiscard]] auto index(StaticTableGroupId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<TypeRefId> type;
    std::vector<SchemaRange<StringId>> columns;
};

}
