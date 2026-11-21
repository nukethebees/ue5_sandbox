#pragma once

#include <material_gen/MaterialValueType.h>

#include <cstddef>
namespace material_synth {

auto is_numeric(ValueType type) -> bool;
auto component_count(ValueType type) -> std::size_t;

}
