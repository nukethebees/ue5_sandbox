#include "TestResultAssetIO.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <HAL/FileManager.h>
#include <Misc/AutomationTest.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <UObject/UObjectGlobals.h>

namespace ml {
FTestResultAsset::FTestResultAsset(FName const test_name, FAutomationTestBase& in_test_runner)
    : package_name{FString::Printf(TEXT("/Game/test_results/%s"), *test_name.ToString())}
    , asset_name{test_name}
    , test_runner{&in_test_runner} {}

auto FTestResultAsset::load_or_create(UClass* const asset_class) const -> UObject* {
    auto const package_path{package_name.ToString()};
    auto* package{FPackageName::DoesPackageExist(package_path)
                      ? LoadPackage(nullptr, *package_path, LOAD_None)
                      : CreatePackage(*package_path)};
    check(package);
    package->FullyLoad();

    auto* asset{FindObject<UObject>(package, *asset_name.ToString())};
    if (!asset) {
        asset = NewObject<UObject>(package, asset_class, asset_name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(asset);
    }

    check(asset->IsA(asset_class));
    return asset;
}

void FTestResultAsset::save(UObject& asset) const {
    auto* package{asset.GetOutermost()};
    check(package);
    check(package->GetFName() == package_name);

    package->MarkPackageDirty();

    auto const output_path{FPackageName::LongPackageNameToFilename(
        package_name.ToString(), FPackageName::GetAssetPackageExtension())};
    auto const output_directory{FPaths::GetPath(output_path)};
    IFileManager::Get().MakeDirectory(*output_directory, true);

    FSavePackageArgs save_args{};
    save_args.TopLevelFlags = RF_Public | RF_Standalone;

    auto const was_saved{UPackage::SavePackage(package, &asset, *output_path, save_args)};

    if (was_saved) {
        test_runner->AddInfo(
            FString::Printf(TEXT("Exported test result asset to %s"), *output_path));
    } else {
        test_runner->AddInfo(
            FString::Printf(TEXT("Failed to export test result asset to %s"), *output_path));
    }
}
}
