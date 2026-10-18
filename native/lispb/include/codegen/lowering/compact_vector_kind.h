#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class CompactVectorKind : std::uint8_t { vector2, vector3, rotator };

auto compact_vector_prefix(CompactVectorKind kind) -> std::string_view;

}
