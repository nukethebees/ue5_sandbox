#pragma once

#include "CoreMinimal.h"
#include "SbxMeshGenLab/MeshData.h"

class UMaterialInterface;
class UStaticMesh;

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    write_static_mesh_asset(FSbxMeshData const& mesh_data,
                            FString const& object_path,
                            UMaterialInterface& material,
                            FString const& generation_description = {}) -> UStaticMesh*;

}
