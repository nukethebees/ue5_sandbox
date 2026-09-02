#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

namespace ml::s7 {
struct SPACEGAMES7_API FLevelScriptEntry {
    FString filename{};
    FString path{};
    FString display_title{};
    FString description{};
    FString error{};
    TOptional<FLevelDefinition> definition{NullOpt};

    explicit operator bool() const noexcept { return definition.IsSet(); }
};

struct SPACEGAMES7_API FLevelScriptCatalogResult {
    FString directory{};
    FString error{};
    TArray<FLevelScriptEntry> entries{};
};

SPACEGAMES7_API auto default_level_script_directory() -> FString;
SPACEGAMES7_API auto discover_level_scripts(FStringView directory) -> FLevelScriptCatalogResult;
SPACEGAMES7_API auto discover_level_scripts() -> FLevelScriptCatalogResult;
}
