#pragma once

#include <ioj/constrained_vectors.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

#include <string_view>

namespace mesh_gen {

struct BoxInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        return ioj::PositiveFiniteVector<float, 3>::valid(dimensions)
                 ? ""
                 : "All box dimensions must be finite and greater than zero.";
    }

    Vec3f dimensions{100.0f, 100.0f, 100.0f};
};
using BoxParameters = CheckedParameters<BoxInput>;

[[nodiscard]] auto generate_box(BoxParameters const& parameters = {}) -> MeshData;

}
