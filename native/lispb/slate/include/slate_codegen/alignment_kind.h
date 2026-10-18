#pragma once

#include <cstdint>
namespace slate_codegen::detail {

enum class AlignmentKind : std::uint8_t { none, start, center, end, fill, parameter };

}
