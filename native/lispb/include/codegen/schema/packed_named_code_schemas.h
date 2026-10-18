#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct PackedNamedCodeSchemas {
    [[nodiscard]] auto append() -> PackedNamedCodeId;
    [[nodiscard]] auto clone(PackedNamedCodeId id) -> PackedNamedCodeId;
    [[nodiscard]] auto index(PackedNamedCodeId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<PackedIntegerValue> value;
    std::vector<std::uint8_t> sentinel;
};

}
