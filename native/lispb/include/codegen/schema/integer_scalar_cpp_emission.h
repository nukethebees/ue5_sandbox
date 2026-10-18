#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class IntegerScalarCppEmission : std::uint8_t { none, constants, constants_with_names, alias };

[[nodiscard]] constexpr auto
    integer_scalar_cpp_emission_name(IntegerScalarCppEmission const emission) -> std::string_view {
    switch (emission) {
        case IntegerScalarCppEmission::none:
            return "none";
        case IntegerScalarCppEmission::constants:
            return "constants";
        case IntegerScalarCppEmission::constants_with_names:
            return "constants-with-names";
        case IntegerScalarCppEmission::alias:
            return "alias";
    }
    return "none";
}

} // namespace lispb::codegen
