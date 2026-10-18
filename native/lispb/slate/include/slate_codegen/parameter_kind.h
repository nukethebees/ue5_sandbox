#pragma once

#include <cstdint>
namespace slate_codegen::detail {

enum class ParameterKind : std::uint8_t { value, callback, factory, existing };

}
