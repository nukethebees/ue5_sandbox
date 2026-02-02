#include "SandboxEditor/checks/describable_checks.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "components/PrimitiveComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/interaction/Describable.h"
#include "SandboxEditor/logging/SandboxEditorLogCategories.h"

#define LOG(VERBOSITY, MSG, ...) \
    UE_LOG(LogSandboxEditorChecks, VERBOSITY, TEXT(MSG) __VA_OPT__(, ) __VA_ARGS__);

namespace ml {
void check_describable_actors_are_visible_to_hitscan() {
    LOG(Log, "Checking IDescribable actors.");

    auto& asset_registry{
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get()};

    FARFilter asset_reg_filter;
    // Get all blueprints
    asset_reg_filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    asset_reg_filter.bRecursiveClasses = true;
    asset_reg_filter.bRecursivePaths = true;

    FString const game_dir{"/Game"};
    TArray<FString> asset_dirs;
    TArray<FString> to_exclude{TEXT("/UltraDynamicSky"), TEXT("/Fab")};
    asset_registry.GetSubPaths(*game_dir, asset_dirs, false);
    asset_dirs = asset_dirs.FilterByPredicate([&](FString const& fs) -> bool {
        for (auto const& x : to_exclude) {
            if ((fs == game_dir + x) || fs.StartsWith(game_dir + "/__")) {
                return false;
            }
        }

        return true;
    });

    if (asset_dirs.IsEmpty()) {
        LOG(Warning, "No directories to scan.");
        return;
    }

    LOG(Verbose, "Scanning %d directories", asset_dirs.Num());
    for (auto const& asset_dir : asset_dirs) {
        LOG(Verbose, "    %s", *asset_dir);
        asset_reg_filter.PackagePaths.Add(*asset_dir);
    }

    TArray<FAssetData> blueprints;
    asset_registry.GetAssets(asset_reg_filter, blueprints);

    LOG(Verbose, "Found %d candidate BPs.", blueprints.Num());

    int32 count{0};
    for (auto const& asset : blueprints) {
        LOG(VeryVerbose, "Checking blueprint: %s", *asset.GetFullName());

        auto* bp{Cast<UBlueprint>(asset.GetAsset())};
        if (!bp) {
            continue;
        }

        if (!bp->GeneratedClass) {
            continue;
        }
        LOG(VeryVerbose, "    Generated class: %s", *bp->GeneratedClass->GetName());

        auto* cdo{bp->GeneratedClass->GetDefaultObject()};
        if (!cdo) {
            continue;
        }
        LOG(VeryVerbose, "    CDO: %s", *cdo->GetName());

        auto* actor{Cast<AActor>(cdo)};
        if (!actor) {
            continue;
        }
        if (!Cast<IDescribable>(actor)) {
            continue;
        }

        auto const actor_name{ml::get_best_display_name(*actor)};

        if (!describable_actor_can_be_hitcan(*actor)) {
            LOG(Warning, "%s cannot be seen.", *actor_name);
        } else {
            LOG(Log, "%s can be seen.", *actor_name);
        }

        ++count;
    }

    LOG(Log, "Found %d BPs that implement IDescribable", count);
}

bool describable_actor_can_be_hitcan(AActor& actor) {
    auto const components{actor.GetComponents().Array()};
    for (auto const* component : components) {
        auto const* prim_comp{Cast<UPrimitiveComponent>(component)};
        if (!prim_comp) {
            continue;
        }

        auto const response{prim_comp->GetCollisionResponseToChannel(ml::collision::description)};
        if (response == ECollisionResponse::ECR_Block) {
            return true;
        }
    }

    return false;
}
}
