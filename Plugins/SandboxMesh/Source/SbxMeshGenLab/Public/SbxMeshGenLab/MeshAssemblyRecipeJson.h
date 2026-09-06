#pragma once

#include "SbxMeshGenLab/MeshAssemblyRecipe.h"

#include "MeshAssemblyRecipeJson.generated.h"

USTRUCT()
struct SBXMESHGENLAB_API FSbxMeshAssemblyRecipeJsonDocument {
    GENERATED_BODY()

    UPROPERTY()
    int32 format_version{1};

    UPROPERTY()
    FName recipe_name{TEXT("SMR_NewAssembly")};

    UPROPERTY()
    FName output_asset_name{TEXT("SM_GeneratedAssembly")};

    UPROPERTY()
    TArray<FSbxMeshAssemblyRecipePart> parts;

    UPROPERTY()
    TArray<FSbxMeshAssemblyRecipeGroup> groups;
};

namespace SandboxMesh {

[[nodiscard]] SBXMESHGENLAB_API auto
    validate_mesh_assembly_recipe_json(FSbxMeshAssemblyRecipeJsonDocument const& document)
        -> FString;
[[nodiscard]] SBXMESHGENLAB_API auto serialize_mesh_assembly_recipe_json(
    FSbxMeshAssemblyRecipeJsonDocument const& document, FString& json, FString& error) -> bool;
[[nodiscard]] SBXMESHGENLAB_API auto deserialize_mesh_assembly_recipe_json(
    FString const& json, FSbxMeshAssemblyRecipeJsonDocument& document, FString& error) -> bool;
[[nodiscard]] SBXMESHGENLAB_API auto
    save_mesh_assembly_recipe_json(FString const& filename,
                                   FSbxMeshAssemblyRecipeJsonDocument const& document,
                                   FString& error) -> bool;
[[nodiscard]] SBXMESHGENLAB_API auto load_mesh_assembly_recipe_json(
    FString const& filename, FSbxMeshAssemblyRecipeJsonDocument& document, FString& error) -> bool;

}
