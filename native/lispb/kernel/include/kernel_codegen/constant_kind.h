#pragma once

#include <cstdint>
namespace kernel_codegen::detail {

enum class ConstantKind : std::uint8_t { nan, infinity, negative_infinity };

}
