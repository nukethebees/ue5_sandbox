#include "Commandlets/GenerateLuaLabAssetsCommandlet.h"

#include "LuaLab/Interpreter.h"
#include "LuaLabEditor/LuaLabWorkbench.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace {
FString const package_name{TEXT("/SbxLangLab/Examples/EUW_LuaLabWorkbench")};
FName const asset_name{TEXT("EUW_LuaLabWorkbench")};

bool validate_example_scripts() {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SbxLangLab"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("Could not find SbxLangLab while validating Lua scripts."));
        return false;
    }

    auto const script_directory{
        FPaths::Combine(plugin->GetBaseDir(), TEXT("Scripts"), TEXT("LuaExamples"))};
    TArray<FString> filenames;
    IFileManager::Get().FindFiles(
        filenames, *FPaths::Combine(script_directory, TEXT("*.lua")), true, false);
    filenames.Sort();
    if (filenames.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("No LuaLab example scripts were found."));
        return false;
    }

    LuaLab::FInterpreter interpreter;
    for (FString const& filename : filenames) {
        FString source;
        auto const path{FPaths::Combine(script_directory, filename)};
        if (!FFileHelper::LoadFileToString(source, *path)) {
            UE_LOG(LogTemp, Error, TEXT("Could not read LuaLab example '%s'."), *path);
            return false;
        }

        auto const result{interpreter.evaluate(source)};
        if (!result.succeeded) {
            UE_LOG(
                LogTemp, Error, TEXT("LuaLab example '%s' failed: %s"), *filename, *result.error);
            return false;
        }

        UE_LOG(LogTemp, Display, TEXT("LuaLab example '%s' returned %s"), *filename, *result.value);
    }

    return true;
}
}

UGenerateLuaLabAssetsCommandlet::UGenerateLuaLabAssetsCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UGenerateLuaLabAssetsCommandlet::Main(FString const&) {
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString())};
    if (LoadObject<UObject>(nullptr, *object_path) != nullptr) {
        UE_LOG(LogTemp, Display, TEXT("LuaLab workbench asset already exists."));
    } else {
        auto* const package{CreatePackage(*package_name)};
        auto* const factory{NewObject<UEditorUtilityWidgetBlueprintFactory>()};
        factory->ParentClass = ULuaLabWorkbench::StaticClass();

        auto* const asset{factory->FactoryCreateNew(factory->SupportedClass,
                                                    package,
                                                    asset_name,
                                                    RF_Public | RF_Standalone,
                                                    nullptr,
                                                    GWarn)};
        if (asset == nullptr) {
            UE_LOG(LogTemp, Error, TEXT("Failed to create the LuaLab workbench asset."));
            return 1;
        }

        FAssetRegistryModule::AssetCreated(asset);
        package->MarkPackageDirty();

        auto const filename{FPackageName::LongPackageNameToFilename(
            package_name, FPackageName::GetAssetPackageExtension())};
        FSavePackageArgs save_args;
        save_args.TopLevelFlags = RF_Public | RF_Standalone;
        save_args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(package, asset, *filename, save_args)) {
            UE_LOG(LogTemp, Error, TEXT("Failed to save '%s'."), *filename);
            return 1;
        }

        UE_LOG(LogTemp, Display, TEXT("Created '%s'."), *filename);
    }

    return validate_example_scripts() ? 0 : 1;
}
