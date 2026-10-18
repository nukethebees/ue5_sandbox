#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class SettingApplyMode : std::uint8_t { immediate, deferred, confirm };

} // namespace lispb::codegen
