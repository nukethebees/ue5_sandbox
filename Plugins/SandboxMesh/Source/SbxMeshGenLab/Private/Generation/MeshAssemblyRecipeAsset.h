#pragma once

#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

#include "CoreMinimal.h"

class USbxMeshAssemblyRecipe;

namespace SandboxMesh {

[[nodiscard]] auto
    write_mesh_assembly_recipe_asset(FName recipe_name,
                                     FName output_asset_name,
                                     TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                     TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
        -> USbxMeshAssemblyRecipe*;
[[nodiscard]] auto
    write_generated_mesh_assembly_recipe_asset(FName recipe_name,
                                               FName output_asset_name,
                                               TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                               TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
        -> USbxMeshAssemblyRecipe*;

}
