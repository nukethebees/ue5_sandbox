#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionImplementationKind : std::uint8_t { none, body, defaulted, deleted };

}
