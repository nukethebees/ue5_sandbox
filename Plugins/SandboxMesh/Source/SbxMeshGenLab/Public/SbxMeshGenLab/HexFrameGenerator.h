#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxHexFrameParameters {
    float outer_radius{50.0f};
    float wall_thickness{10.0f};
    float depth{20.0f};
    bool pointy_top{false};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    generate_hex_frame(FSbxHexFrameParameters const& parameters = {}) -> FSbxMeshData;

}
