#pragma once
#include <cstdint>

namespace lispb::codegen::sexpr {
enum class Dialect : std::uint8_t { lispb, scheme };
}
