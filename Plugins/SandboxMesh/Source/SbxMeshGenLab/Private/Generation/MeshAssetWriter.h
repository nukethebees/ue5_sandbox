#pragma once

#include "CoreMinimal.h"
#include "SbxMeshGenLab/MeshData.h"

class UStaticMesh;

namespace SandboxMesh {

[[nodiscard]] auto get_generated_asset_filename(FName asset_name) -> FString;
[[nodiscard]] auto get_generated_asset_object_path(FName asset_name) -> FString;
[[nodiscard]] auto create_transient_static_mesh(FSbxMeshData const& mesh_data) -> UStaticMesh*;
[[nodiscard]] auto write_generated_static_mesh_asset(FSbxMeshData const& mesh_data,
                                                     FName asset_name,
                                                     FString const& generation_description = {})
    -> UStaticMesh*;

}
