#pragma once

#include <mesh_gen/mesh_data.h>

#include <cstdint>

namespace mesh_gen {

struct CylinderParameters {
    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};

[[nodiscard]] auto generate_cylinder(CylinderParameters const& parameters = {}) -> MeshData;

}
