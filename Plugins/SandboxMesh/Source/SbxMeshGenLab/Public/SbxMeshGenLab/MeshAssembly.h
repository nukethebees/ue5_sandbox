#pragma once

#include "SbxMeshGenLab/MeshGenerationRequest.h"

using FSbxMeshTransform = mesh_gen::Transform;
using FSbxMeshAssemblyPart = mesh_gen::AssemblyPart;

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    validate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto
    generate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FSbxMeshData;
[[nodiscard]] SBXMESHGENLAB_API auto
    describe_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString;

using mesh_gen::append_transformed_mesh;

}
