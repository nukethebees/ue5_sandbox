#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionKind : std::uint8_t { ordinary, constructor, explicit_constructor, destructor };

}
