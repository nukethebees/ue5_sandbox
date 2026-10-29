#pragma once

#include <ioj/levels/authoring/level_source.h>
#include <ioj/settings/settings.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <CoreMinimal.h>

#include <expected>

class USpaceGameLevelConfig;

struct FLoadedSettingsSource {
    ioj::settings::Settings settings;
    FString source;
    FString evaluated_source;
};

struct FLoadedGameSettings {
    USpaceGameLevelConfig* config{};
    ioj::sim::SimulationSettings simulation;
    FString source;
    FString level_source;
    std::optional<ioj::levels::LevelDefinition> level;
};

[[nodiscard]] SPACEGAME_API auto read_game_settings_source(FStringView path,
                                                           USpaceGameLevelConfig const& resources)
    -> std::expected<FLoadedSettingsSource, FString>;

// Require an outer that owns the resolved runtime configuration.
// Prebuilt levels retain their resolved settings, ignore path and return empty source text.
[[nodiscard]] SPACEGAME_API auto
    read_game_settings(FStringView path,
                       ioj::levels::authoring::LevelSource const* level,
                       USpaceGameLevelConfig const& resources,
                       UObject* outer) -> std::expected<FLoadedGameSettings, FString>;
