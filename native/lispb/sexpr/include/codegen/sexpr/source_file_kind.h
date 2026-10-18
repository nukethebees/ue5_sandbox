#pragma once

#include <cstdint>

namespace lispb::codegen::sexpr {
enum class SourceFileKind : std::uint8_t { unspecified, type_registry, module };
}
