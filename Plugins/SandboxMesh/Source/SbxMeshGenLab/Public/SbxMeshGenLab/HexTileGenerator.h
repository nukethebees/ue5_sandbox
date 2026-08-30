#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxHexTileParameters {
    float outer_radius{50.0f};
    float depth{20.0f};
    float bevel_width{5.0f};
    bool pointy_top{false};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto generate_hex_tile(FSbxHexTileParameters const& parameters = {})
    -> FSbxMeshData;

}
