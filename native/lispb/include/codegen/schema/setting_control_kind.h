#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class SettingControlKind : std::uint8_t { toggle, choice, float_range, integer_range, custom };

} // namespace lispb::codegen
