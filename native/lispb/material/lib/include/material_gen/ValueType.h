#pragma once

#include <cstddef>
#include <cstdint>
namespace material_synth {

enum class ValueType : std::uint8_t { invalid, float1, float2, float3, float4, texture };

auto is_numeric(ValueType type) -> bool;
auto component_count(ValueType type) -> std::size_t;

}
