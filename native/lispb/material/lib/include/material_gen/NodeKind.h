#pragma once

#include <cstdint>
namespace material_synth {

enum class NodeKind : std::uint8_t {
    constant,
    parameter,
    texture_coordinate,
    per_instance_custom_data,
    add,
    subtract,
    multiply,
    divide,
    lerp,
    saturate,
    sample,
    custom,
    vector_constructor,
    time,
    sine,
    cosine,
    texture_object,
    component_mask,
    world_position,
    object_position,
    pixel_normal,
    vertex_normal,
    camera_vector,
    transform_position,
    scene_texture,
    shader_call,
    camera_position,
    step,
    vertex_color,
};

}
