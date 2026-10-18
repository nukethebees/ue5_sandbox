#pragma once

#include <cstdint>
namespace kernel_codegen::detail {

enum class ExpressionKind : std::uint8_t { reference, literal, constant, binary };

}
