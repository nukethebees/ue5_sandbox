#pragma once

#include <cstdint>
#include <string>
namespace kernel_codegen::detail {

enum class ScalarType : std::uint8_t { int32, uint32, float32, float64 };

auto type_spelling(ScalarType type) -> std::string;

}
