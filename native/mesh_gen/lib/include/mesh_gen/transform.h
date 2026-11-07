#pragma once

#include <ioj/constrained_vectors.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/rotatorf.h>
#include <mesh_gen/vector_types.h>

#include <string_view>

namespace mesh_gen {

struct TransformInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        return ioj::PositiveFiniteVector<float, 3>::valid(scale)
                 ? ""
                 : "all scale values must be finite and greater than zero.";
    }

    Vec3f translation{};
    Rotatorf rotation{};
    Vec3f scale{1.0f, 1.0f, 1.0f};
};
using Transform = CheckedParameters<TransformInput>;

}
