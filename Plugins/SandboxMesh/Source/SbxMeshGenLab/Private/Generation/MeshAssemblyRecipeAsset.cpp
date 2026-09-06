#include "Generation/MeshAssemblyRecipeAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SbxMeshGenLab/MeshAssemblyRecipe.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshAssemblyRecipe, Log, All);

namespace SandboxMesh {
namespace {
FString const recipe_package_path{TEXT("/SandboxMesh/MeshGenLab/Recipes")};
FString const generated_package_path{TEXT("/SandboxMesh/MeshGenLab/Generated")};

auto ensure_content_directory(FString const& subdirectory) -> bool {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxMesh"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogSbxMeshAssemblyRecipe, Error, TEXT("SandboxMesh plugin was not found."));
        return false;
    }

    auto const output_directory{
        FPaths::Combine(plugin->GetContentDir(), TEXT("MeshGenLab"), subdirectory)};
    auto& file_manager{IFileManager::Get()};
    if (!file_manager.MakeDirectory(*output_directory, true) &&
        !file_manager.DirectoryExists(*output_directory)) {
        UE_LOG(LogSbxMeshAssemblyRecipe,
               Error,
               TEXT("Failed to create the mesh recipe directory: %s"),
               *output_directory);
        return false;
    }
    return true;
}

auto write_recipe_asset(FString const& package_path,
                        FString const& content_subdirectory,
                        FName const recipe_name,
                        FName const output_asset_name,
                        TArray<FSbxMeshAssemblyRecipePart> const& parts,
                        TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
    -> USbxMeshAssemblyRecipe* {
    auto const recipe_name_string{recipe_name.ToString()};
    auto const package_name{FString::Printf(TEXT("%s/%s"), *package_path, *recipe_name_string)};
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, *recipe_name_string)};
    FText invalid_name_reason;
    if (recipe_name.IsNone() ||
        !FPackageName::IsValidObjectPath(object_path, &invalid_name_reason)) {
        UE_LOG(LogSbxMeshAssemblyRecipe,
               Error,
               TEXT("Invalid mesh recipe name '%s': %s"),
               *recipe_name_string,
               *invalid_name_reason.ToString());
        return nullptr;
    }
    if (!ensure_content_directory(content_subdirectory)) {
        return nullptr;
    }

    auto* recipe{LoadObject<USbxMeshAssemblyRecipe>(nullptr, *object_path, nullptr, LOAD_NoWarn)};
    auto const is_new_asset{recipe == nullptr};
    auto* const package{is_new_asset ? CreatePackage(*package_name) : recipe->GetOutermost()};
    if (package == nullptr) {
        UE_LOG(LogSbxMeshAssemblyRecipe,
               Error,
               TEXT("Failed to create the mesh recipe package: %s"),
               *package_name);
        return nullptr;
    }

    if (is_new_asset) {
        recipe = NewObject<USbxMeshAssemblyRecipe>(
            package, recipe_name, RF_Public | RF_Standalone | RF_Transactional);
    }
    if (recipe == nullptr) {
        UE_LOG(LogSbxMeshAssemblyRecipe, Error, TEXT("Failed to create the mesh recipe asset."));
        return nullptr;
    }

    recipe->Modify();
    recipe->set_hierarchy(output_asset_name, parts, groups);
    recipe->MarkPackageDirty();
    if (is_new_asset) {
        FAssetRegistryModule::AssetCreated(recipe);
    }

    FSavePackageArgs save_arguments;
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    auto const package_filename{FPackageName::LongPackageNameToFilename(
        package_name, FPackageName::GetAssetPackageExtension())};
    if (!UPackage::SavePackage(package, recipe, *package_filename, save_arguments)) {
        UE_LOG(LogSbxMeshAssemblyRecipe,
               Error,
               TEXT("Failed to save mesh recipe asset: %s"),
               *package_filename);
        return nullptr;
    }

    UE_LOG(LogSbxMeshAssemblyRecipe,
           Display,
           TEXT("Saved mesh assembly recipe: %s"),
           *recipe->GetPathName());
    return recipe;
}
}

auto write_mesh_assembly_recipe_asset(FName const recipe_name,
                                      FName const output_asset_name,
                                      TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                      TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
    -> USbxMeshAssemblyRecipe* {
    return write_recipe_asset(
        recipe_package_path, TEXT("Recipes"), recipe_name, output_asset_name, parts, groups);
}

auto write_generated_mesh_assembly_recipe_asset(FName const recipe_name,
                                                FName const output_asset_name,
                                                TArray<FSbxMeshAssemblyRecipePart> const& parts,
                                                TArray<FSbxMeshAssemblyRecipeGroup> const& groups)
    -> USbxMeshAssemblyRecipe* {
    return write_recipe_asset(
        generated_package_path, TEXT("Generated"), recipe_name, output_asset_name, parts, groups);
}

}
