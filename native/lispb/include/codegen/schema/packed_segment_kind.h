#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class PackedSegmentKind : std::uint8_t {
    field,
    reserved,
};

} // namespace lispb::codegen
