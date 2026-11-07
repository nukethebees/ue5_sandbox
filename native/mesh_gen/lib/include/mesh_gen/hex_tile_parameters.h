#pragma once

#include <mesh_gen/mesh_data.h>

namespace mesh_gen {

struct HexTileParameters {
    float outer_radius{50.0f};
    float depth{20.0f};
    float bevel_width{5.0f};
    bool pointy_top{};
};

[[nodiscard]] auto generate_hex_tile(HexTileParameters const& parameters = {}) -> MeshData;

}
