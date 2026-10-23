#pragma once
#include <cstdint>

namespace lispb::codegen {
enum class CppDeclarationKind : std::uint8_t {
    environment,
    function,
    parameter,
    type_parameter,
    value_parameter,
    node_begin = 32,
};
}
