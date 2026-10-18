#pragma once
#include <cstdint>

namespace lispb::codegen::sexpr {
enum class ExprKind : std::uint8_t { invalid, atom, string, list, quote, comment };
}
