#pragma once

#include "SbxMeshGenLab/MeshData.h"

struct FSbxSphereParameters {
    float radius{50.0f};
    int32 longitude_segments{32};
    int32 latitude_segments{16};
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto generate_sphere(FSbxSphereParameters const& parameters = {})
    -> FSbxMeshData;

}
