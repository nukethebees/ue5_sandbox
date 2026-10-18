#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class TypeReferenceKind : std::uint8_t {
    ordinary,
    function_declaration,
    function_definition,
    complete_definition,
};

} // namespace lispb::codegen
