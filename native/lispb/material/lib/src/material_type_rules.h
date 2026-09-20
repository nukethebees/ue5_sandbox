#pragma once

#include <material_gen/MaterialIR.h>

#include <string_view>

namespace material_synth {

auto valid_identifier(std::string_view value) -> bool;
auto promote_value_types(ValueType left, ValueType right) -> ValueType;

}
