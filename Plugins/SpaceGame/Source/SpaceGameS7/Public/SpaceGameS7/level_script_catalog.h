#pragma once

#include <ioj/presentation/level_catalog.h>

#include <CoreMinimal.h>

namespace ioj::levels::authoring {
struct SPACEGAMES7_API FLevelScriptEntry {
    FString filename{};
    FString path{};
    FString source_text{};
    FString display_title{};
    FString description{};
    FString error{};
    levels::LevelId id{};
    std::optional<float> par_time{};
    LevelCatalogCategory category{};
    int32 team_count{};
    int32 entity_count{};
    bool has_player{};
    profiles::LevelProgressSummary progress{};
    profiles::CompletionIndicatorState indicator{};
    profiles::UnlockStatus prerequisites{};
    bool unlocked{};

    explicit operator bool() const noexcept { return !id.empty(); }
};

SPACEGAMES7_API auto default_level_script_directory() -> FString;
}
