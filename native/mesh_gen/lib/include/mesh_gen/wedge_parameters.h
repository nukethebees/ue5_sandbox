#pragma once

#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

namespace mesh_gen {

struct WedgeParameters {
    Vec3f dimensions{100.0f, 100.0f, 50.0f};
    float top_length{50.0f};
    float top_offset{};
};

[[nodiscard]] auto generate_wedge(WedgeParameters const& parameters = {}) -> MeshData;

}
