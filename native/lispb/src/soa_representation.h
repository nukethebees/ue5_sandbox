#pragma once

#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen::detail {

enum class SoaRepresentation : std::uint8_t { vector, native_vector_view, compact };

}
