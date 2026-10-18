#pragma once

#include <cstdint>
namespace kernel_codegen::detail {

enum class Aliasing : std::uint8_t { output_disjoint, pairwise_disjoint };

}
