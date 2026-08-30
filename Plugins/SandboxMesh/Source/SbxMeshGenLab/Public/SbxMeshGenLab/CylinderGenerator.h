#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxCylinderParameters {
    float radius{50.0f};
    float height{100.0f};
    int32 radial_segments{32};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    generate_cylinder(FSbxCylinderParameters const& parameters = {}) -> FSbxMeshData;

}
