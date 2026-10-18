#pragma once

#include <cstdint>

namespace lispb::schema {

enum class PackedSegmentKind : std::uint8_t { field = 1, reserved };

} // namespace lispb::schema
