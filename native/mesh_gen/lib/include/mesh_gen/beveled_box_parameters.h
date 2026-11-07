#pragma once

#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

namespace mesh_gen {

struct BeveledBoxParameters {
    Vec3f dimensions{100.0f, 100.0f, 100.0f};
    float bevel_width{10.0f};
};

[[nodiscard]] auto generate_beveled_box(BeveledBoxParameters const& parameters = {}) -> MeshData;

}
