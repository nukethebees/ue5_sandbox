#pragma once

#include <cstdint>

namespace lispb::codegen::sexpr {

enum class SourceFileState : std::uint8_t { stale, current, overlay };

}
