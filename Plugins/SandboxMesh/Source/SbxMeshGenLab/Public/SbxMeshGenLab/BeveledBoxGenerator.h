#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxBeveledBoxParameters {
    FVector3f dimensions{100.0f, 100.0f, 100.0f};
    float bevel_width{10.0f};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    generate_beveled_box(FSbxBeveledBoxParameters const& parameters = {}) -> FSbxMeshData;

}
