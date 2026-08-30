#pragma once

#include "SbxMeshGenLab/MeshGenerationRequest.h"

struct FSbxMeshTransform {
    FVector3f translation{FVector3f::ZeroVector};
    FRotator3f rotation{FRotator3f::ZeroRotator};
    FVector3f scale{FVector3f::OneVector};
};

struct FSbxMeshAssemblyPart {
    FSbxMeshGenerationRequest mesh;
    FSbxMeshTransform transform;
};

namespace SandboxMesh {

SBXMESHGENLAB_API void append_transformed_mesh(FSbxMeshData& destination,
                                               FSbxMeshData const& source,
                                               FSbxMeshTransform const& transform);
[[nodiscard]] SBXMESHGENLAB_API auto
    validate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto
    generate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FSbxMeshData;
[[nodiscard]] SBXMESHGENLAB_API auto
    describe_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString;

}
