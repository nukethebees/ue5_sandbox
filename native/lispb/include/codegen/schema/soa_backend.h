#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class SoaBackend : std::uint8_t {
    unreal,
    standard_library,
};

} // namespace lispb::codegen
