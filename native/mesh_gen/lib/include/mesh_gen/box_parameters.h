#pragma once

#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

namespace mesh_gen {

struct BoxParameters {
    Vec3f dimensions{100.0f, 100.0f, 100.0f};
};

[[nodiscard]] auto generate_box(BoxParameters const& parameters = {}) -> MeshData;

}
