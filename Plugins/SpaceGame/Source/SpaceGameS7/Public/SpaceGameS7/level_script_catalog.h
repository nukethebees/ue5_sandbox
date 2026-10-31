#pragma once

#include <ioj/levels/authoring/definition_catalog.h>

#include <CoreMinimal.h>

namespace ioj::levels::authoring {
struct SPACEGAMES7_API FLevelScriptEntry {
    FString filename{};
    FString path{};
    FString source_text{};
    FString display_title{};
    FString description{};
    FString error{};
    TOptional<levels::LevelDefinition> definition{NullOpt};

    explicit operator bool() const noexcept { return definition.IsSet(); }
};

struct SPACEGAMES7_API FCampaignScriptEntry {
    FString filename{};
    FString path{};
    FString error{};
    TOptional<CampaignDefinition> definition{NullOpt};

    explicit operator bool() const noexcept { return definition.IsSet(); }
};

struct SPACEGAMES7_API FLevelScriptCatalogResult {
    FString directory{};
    FString error{};
    TArray<FLevelScriptEntry> entries{};
    TArray<FCampaignScriptEntry> campaigns{};
};

SPACEGAMES7_API auto default_level_script_directory() -> FString;
SPACEGAMES7_API auto default_campaign_script_directory() -> FString;
SPACEGAMES7_API auto default_level_script_root() -> FString;
SPACEGAMES7_API auto load_level_script_catalog(FStringView root_path) -> FLevelScriptCatalogResult;
SPACEGAMES7_API auto load_level_script_catalog() -> FLevelScriptCatalogResult;
}
