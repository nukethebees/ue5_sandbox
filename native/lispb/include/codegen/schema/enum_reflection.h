#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class EnumReflection : std::uint8_t {
    none,
    uenum,
    blueprint,
};

inline constexpr auto all_enum_reflections() -> std::array<EnumReflection, 3> {
    return {EnumReflection::none, EnumReflection::uenum, EnumReflection::blueprint};
}

inline constexpr auto enum_reflection_name(EnumReflection const reflection) -> std::string_view {
    switch (reflection) {
        case EnumReflection::none:
            return "none";
        case EnumReflection::uenum:
            return "uenum";
        case EnumReflection::blueprint:
            return "blueprint";
    }
    return "unknown";
}

} // namespace lispb::codegen
