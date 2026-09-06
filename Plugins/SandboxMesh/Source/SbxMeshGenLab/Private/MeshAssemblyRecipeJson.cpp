#include "SbxMeshGenLab/MeshAssemblyRecipeJson.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SandboxMesh {

auto validate_mesh_assembly_recipe_json(FSbxMeshAssemblyRecipeJsonDocument const& document)
    -> FString {
    if (document.format_version != 1) {
        return FString::Printf(TEXT("JSON recipe format version %d is not supported."),
                               document.format_version);
    }
    if (document.recipe_name.IsNone()) {
        return TEXT("JSON recipe name must not be empty.");
    }
    if (document.output_asset_name.IsNone()) {
        return TEXT("JSON output asset name must not be empty.");
    }

    auto const hierarchy_error{validate_mesh_assembly_hierarchy(document.parts, document.groups)};
    if (!hierarchy_error.IsEmpty()) {
        return hierarchy_error;
    }

    auto const parts{resolve_mesh_assembly_hierarchy(
        document.parts, document.groups, document.output_asset_name)};
    return validate_mesh_assembly(parts);
}

auto serialize_mesh_assembly_recipe_json(FSbxMeshAssemblyRecipeJsonDocument const& document,
                                         FString& json,
                                         FString& error) -> bool {
    error = validate_mesh_assembly_recipe_json(document);
    if (!error.IsEmpty()) {
        return false;
    }
    if (!FJsonObjectConverter::UStructToJsonObjectString(document, json)) {
        error = TEXT("Failed to serialize the mesh assembly recipe as JSON.");
        return false;
    }
    return true;
}

auto deserialize_mesh_assembly_recipe_json(FString const& json,
                                           FSbxMeshAssemblyRecipeJsonDocument& document,
                                           FString& error) -> bool {
    FText conversion_error;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(
            json, &document, 0, 0, false, &conversion_error)) {
        error = conversion_error.ToString();
        return false;
    }

    error = validate_mesh_assembly_recipe_json(document);
    return error.IsEmpty();
}

auto save_mesh_assembly_recipe_json(FString const& filename,
                                    FSbxMeshAssemblyRecipeJsonDocument const& document,
                                    FString& error) -> bool {
    FString json;
    if (!serialize_mesh_assembly_recipe_json(document, json, error)) {
        return false;
    }

    auto const directory{FPaths::GetPath(filename)};
    auto& file_manager{IFileManager::Get()};
    if (!directory.IsEmpty() && !file_manager.MakeDirectory(*directory, true) &&
        !file_manager.DirectoryExists(*directory)) {
        error = FString::Printf(TEXT("Failed to create JSON recipe directory: %s"), *directory);
        return false;
    }
    if (!FFileHelper::SaveStringToFile(
            json, *filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        error = FString::Printf(TEXT("Failed to save JSON recipe: %s"), *filename);
        return false;
    }
    return true;
}

auto load_mesh_assembly_recipe_json(FString const& filename,
                                    FSbxMeshAssemblyRecipeJsonDocument& document,
                                    FString& error) -> bool {
    FString json;
    if (!FFileHelper::LoadFileToString(json, *filename)) {
        error = FString::Printf(TEXT("Failed to read JSON recipe: %s"), *filename);
        return false;
    }
    return deserialize_mesh_assembly_recipe_json(json, document, error);
}

}
