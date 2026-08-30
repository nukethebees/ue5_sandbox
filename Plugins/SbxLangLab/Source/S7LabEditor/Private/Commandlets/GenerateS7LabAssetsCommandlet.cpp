#include "Commandlets/GenerateS7LabAssetsCommandlet.h"

#include "S7Lab/Interpreter.h"
#include "S7LabEditor/S7LabWorkbench.h"

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
FString const package_name{TEXT("/SbxLangLab/Examples/EUW_S7LabWorkbench")};
FName const asset_name{TEXT("EUW_S7LabWorkbench")};

bool validate_example_scripts() {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SbxLangLab"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("Could not find SbxLangLab while validating scripts."));
        return false;
    }

    auto const script_directory{
        FPaths::Combine(plugin->GetBaseDir(), TEXT("Scripts"), TEXT("Examples"))};
    TArray<FString> filenames;
    IFileManager::Get().FindFiles(
        filenames, *FPaths::Combine(script_directory, TEXT("*.scm")), true, false);
    filenames.Sort();
    if (filenames.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("No S7Lab example scripts were found."));
        return false;
    }

    S7Lab::FInterpreter interpreter;
    for (FString const& filename : filenames) {
        FString source;
        auto const path{FPaths::Combine(script_directory, filename)};
        if (!FFileHelper::LoadFileToString(source, *path)) {
            UE_LOG(LogTemp, Error, TEXT("Could not read S7Lab example '%s'."), *path);
            return false;
        }

        auto const result{interpreter.evaluate(source)};
        if (!result.succeeded) {
            UE_LOG(LogTemp, Error, TEXT("S7Lab example '%s' failed: %s"), *filename, *result.error);
            return false;
        }

        UE_LOG(LogTemp, Display, TEXT("S7Lab example '%s' returned %s"), *filename, *result.value);
    }

    return true;
}
}

UGenerateS7LabAssetsCommandlet::UGenerateS7LabAssetsCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UGenerateS7LabAssetsCommandlet::Main(FString const&) {
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString())};
    if (LoadObject<UObject>(nullptr, *object_path) != nullptr) {
        UE_LOG(LogTemp, Display, TEXT("S7Lab workbench asset already exists."));
    } else {
        auto* const package{CreatePackage(*package_name)};
        auto* const factory{NewObject<UEditorUtilityWidgetBlueprintFactory>()};
        factory->ParentClass = US7LabWorkbench::StaticClass();

        auto* const asset{factory->FactoryCreateNew(factory->SupportedClass,
                                                    package,
                                                    asset_name,
                                                    RF_Public | RF_Standalone,
                                                    nullptr,
                                                    GWarn)};
        if (asset == nullptr) {
            UE_LOG(LogTemp, Error, TEXT("Failed to create the S7Lab workbench asset."));
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
