#pragma once

#include "CoreMinimal.h"

struct FSbxMeshData {
    TArray<FVector3f> positions;
    TArray<FVector3f> normals;
    TArray<FVector2f> uvs;
    TArray<uint32> indices;
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto generate_cube(float half_extent = 50.0f) -> FSbxMeshData;

}
