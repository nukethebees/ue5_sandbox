#pragma once

#include <mesh_gen/mesh_data.h>

#include <cstdint>

namespace mesh_gen {

struct SphereParameters {
    float radius{50.0f};
    std::int32_t longitude_segments{32};
    std::int32_t latitude_segments{16};
};

[[nodiscard]] auto generate_sphere(SphereParameters const& parameters = {}) -> MeshData;

}
