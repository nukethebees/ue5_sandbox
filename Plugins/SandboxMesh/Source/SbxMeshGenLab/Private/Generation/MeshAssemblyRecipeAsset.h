#pragma once

#include "CoreMinimal.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

class USbxMeshAssemblyRecipe;

namespace SandboxMesh {

[[nodiscard]] auto
    write_mesh_assembly_recipe_asset(FName recipe_name,
                                     FName output_asset_name,
                                     TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
        -> USbxMeshAssemblyRecipe*;

}
