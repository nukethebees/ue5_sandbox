#pragma once

#include <mesh_gen/rotatorf.h>
#include <mesh_gen/vector_types.h>

#include <string_view>

namespace mesh_gen {

struct Transform {
    Vec3f translation{};
    Rotatorf rotation{};
    Vec3f scale{1.0f, 1.0f, 1.0f};
};

[[nodiscard]] auto validate_transform(Transform const& transform) -> std::string_view;

}
