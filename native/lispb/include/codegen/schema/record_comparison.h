#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class RecordComparison : std::uint8_t { none, equality, three_way };

} // namespace lispb::codegen
