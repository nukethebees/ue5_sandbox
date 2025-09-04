#include "Sandbox/utilities/levels.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/TopLevelAssetPath.h"

namespace ml {
auto get_all_level_names(FName level_directory) -> TArray<FName> {
    TArray<FName> level_names;

    auto& asset_registry_module{
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")};
    auto& asset_registry{asset_registry_module.Get()};

    FARFilter filter;
    filter.ClassPaths.Add(FTopLevelAssetPath(FName("/Script/Engine.World")));
    filter.bRecursivePaths = true;
    filter.PackagePaths.Add(level_directory);

    TArray<FAssetData> asset_data;
    asset_registry.GetAssets(filter, asset_data);

    for (auto const& data : asset_data) {
        level_names.Add(data.AssetName); // Or use data.PackageName for full path
    }

    return level_names;
}
}
