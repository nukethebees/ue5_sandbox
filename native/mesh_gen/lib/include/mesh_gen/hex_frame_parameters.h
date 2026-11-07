#pragma once

#include <mesh_gen/hex_frame_uv_mode.h>
#include <mesh_gen/mesh_data.h>

namespace mesh_gen {

struct HexFrameParameters {
    float outer_radius{50.0f};
    float wall_thickness{10.0f};
    float depth{20.0f};
    bool pointy_top{};
    HexFrameUvMode uv_mode{HexFrameUvMode::per_face};
};

[[nodiscard]] auto generate_hex_frame(HexFrameParameters const& parameters = {}) -> MeshData;

}
