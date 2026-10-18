#pragma once

#include <cstdint>
namespace kernel_codegen::detail {

enum class VariantKind : std::uint8_t { out_of_place, in_place };

}
