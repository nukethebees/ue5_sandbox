#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ModuleKind : std::uint8_t {
    normal,
    settings,
    umbrella,
};

} // namespace lispb::codegen
