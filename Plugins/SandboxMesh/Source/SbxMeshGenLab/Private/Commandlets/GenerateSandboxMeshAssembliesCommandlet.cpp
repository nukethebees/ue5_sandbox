#include "Commandlets/GenerateSandboxMeshAssembliesCommandlet.h"

#include "Generation/MeshAssemblyRecipeAsset.h"
#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshAssemblyRecipeJson.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateSandboxMeshAssemblies, Log, All);

namespace {

auto generate_recipe(FString const& filename) -> bool {
    FSbxMeshAssemblyRecipeJsonDocument document;
    FString error;
    if (!SandboxMesh::load_mesh_assembly_recipe_json(filename, document, error)) {
        UE_LOG(LogGenerateSandboxMeshAssemblies,
               Error,
               TEXT("Failed to load '%s': %s"),
               *filename,
               *error);
        return false;
    }

    auto* const recipe{SandboxMesh::write_generated_mesh_assembly_recipe_asset(
        document.recipe_name, document.output_asset_name, document.parts, document.groups)};
    if (recipe == nullptr) {
        return false;
    }

    auto const parts{SandboxMesh::resolve_mesh_assembly_hierarchy(
        document.parts, document.groups, document.output_asset_name)};
    auto const mesh_data{SandboxMesh::generate_mesh_assembly(parts)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, document.output_asset_name, SandboxMesh::describe_mesh_assembly(parts))};
    if (static_mesh == nullptr) {
        return false;
    }

    auto const output_filename{
        SandboxMesh::get_generated_asset_filename(document.output_asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogGenerateSandboxMeshAssemblies,
               Error,
               TEXT("Generated mesh package does not exist: %s"),
               *output_filename);
        return false;
    }

    UE_LOG(LogGenerateSandboxMeshAssemblies,
           Display,
           TEXT("Generated %s and %s from %s."),
           *recipe->GetPathName(),
           *static_mesh->GetPathName(),
           *filename);
    return true;
}

auto find_recipe_files(FString const& parameters, TArray<FString>& filenames) -> bool {
    FString recipe_argument;
    if (FParse::Value(*parameters, TEXT("Recipe="), recipe_argument)) {
        auto filename{recipe_argument};
        if (FPaths::IsRelative(filename)) {
            filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), filename);
        }
        filenames.Add(MoveTemp(filename));
        return true;
    }

    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxMesh"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogGenerateSandboxMeshAssemblies, Error, TEXT("SandboxMesh plugin was not found."));
        return false;
    }

    auto const recipes_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Recipes"))};
    TArray<FString> recipe_names;
    IFileManager::Get().FindFiles(
        recipe_names, *FPaths::Combine(recipes_directory, TEXT("*.json")), true, false);
    recipe_names.Sort();
    for (auto const& recipe_name : recipe_names) {
        filenames.Add(FPaths::Combine(recipes_directory, recipe_name));
    }
    if (filenames.IsEmpty()) {
        UE_LOG(LogGenerateSandboxMeshAssemblies,
               Error,
               TEXT("No JSON recipes found in %s."),
               *recipes_directory);
        return false;
    }
    return true;
}

}

UGenerateSandboxMeshAssembliesCommandlet::UGenerateSandboxMeshAssembliesCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshAssembliesCommandlet::Main(FString const& parameters) {
    TArray<FString> filenames;
    if (!find_recipe_files(parameters, filenames)) {
        return 1;
    }

    bool succeeded{true};
    for (auto const& filename : filenames) {
        succeeded = generate_recipe(filename) && succeeded;
    }
    return succeeded ? 0 : 1;
}
