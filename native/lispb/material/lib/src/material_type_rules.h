#pragma once

#include <material_gen/MaterialIR.h>
#include <material_gen/ValueType.h>

namespace material_synth {

auto promote_value_types(ValueType left, ValueType right) -> ValueType;

}
