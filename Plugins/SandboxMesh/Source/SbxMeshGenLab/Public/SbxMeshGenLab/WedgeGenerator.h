#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxWedgeParameters {
    FVector3f dimensions{100.0f, 100.0f, 50.0f};
    float top_length{50.0f};
    float top_offset{};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto generate_wedge(FSbxWedgeParameters const& parameters = {})
    -> FSbxMeshData;

}
