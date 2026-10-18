#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ExternalScalarKind : std::uint8_t { none, integer, floating };

} // namespace lispb::codegen
