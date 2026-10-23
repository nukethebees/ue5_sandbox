#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct StaticTableRowSchemas {
    [[nodiscard]] auto append() -> StaticTableRowId;
    [[nodiscard]] auto clone(StaticTableRowId id) -> StaticTableRowId;
    [[nodiscard]] auto index(StaticTableRowId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
};

}
