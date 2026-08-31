#pragma once

#include "CoreMinimal.h"
#include "SbxMeshGenLab/MeshAssembly.h"

class USbxMeshAssemblyRecipe;

namespace SandboxMesh {

[[nodiscard]] auto write_mesh_assembly_recipe_asset(FName recipe_name,
                                                    FName output_asset_name,
                                                    TArray<FSbxMeshAssemblyPart> const& parts)
    -> USbxMeshAssemblyRecipe*;

}
