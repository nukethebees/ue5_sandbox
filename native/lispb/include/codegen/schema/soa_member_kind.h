#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class SoaMemberKind : std::uint8_t {
    array,
    nested,
};

} // namespace lispb::codegen
