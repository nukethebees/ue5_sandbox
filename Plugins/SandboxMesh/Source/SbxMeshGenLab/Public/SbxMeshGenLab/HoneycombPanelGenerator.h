#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxHoneycombPanelParameters {
    int32 rows{3};
    int32 columns{4};
    float cell_radius{50.0f};
    float wall_thickness{8.0f};
    float depth{20.0f};
    bool pointy_top{false};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    generate_honeycomb_panel(FSbxHoneycombPanelParameters const& parameters = {}) -> FSbxMeshData;

}
