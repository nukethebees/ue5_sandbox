#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionExceptionKind : std::uint8_t { potentially_throwing, noexcept_, conditional };

}
