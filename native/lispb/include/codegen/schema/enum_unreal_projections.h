#pragma once

#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct EnumUnrealProjections {
    [[nodiscard]] auto append() -> EnumUnrealProjectionId;
    [[nodiscard]] auto clone(EnumUnrealProjectionId id) -> EnumUnrealProjectionId;
    [[nodiscard]] auto index(EnumUnrealProjectionId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<StringId> header;
    std::vector<StringId> header_include;
    std::vector<StringId> conversion_header;
    std::vector<StringId> native_header_include;
    std::vector<EnumReflection> reflection;
    std::vector<std::uint8_t> enum_array;
};

}
