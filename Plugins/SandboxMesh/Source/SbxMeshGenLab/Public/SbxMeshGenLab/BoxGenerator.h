#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxBoxParameters {
    FVector3f dimensions{100.0f, 100.0f, 100.0f};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto generate_box(FSbxBoxParameters const& parameters = {})
    -> FSbxMeshData;

}
