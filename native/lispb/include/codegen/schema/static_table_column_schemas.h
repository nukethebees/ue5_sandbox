#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct StaticTableColumnSchemas {
    [[nodiscard]] auto append() -> StaticTableColumnId;
    [[nodiscard]] auto clone(StaticTableColumnId id) -> StaticTableColumnId;
    [[nodiscard]] auto index(StaticTableColumnId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> type;
};

}
