#pragma once

#include <ioj/settings/settings.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <CoreMinimal.h>

#include <expected>

class USpaceGameLevelConfig;

[[nodiscard]] SPACEGAME_API auto read_game_settings_source(FStringView path,
                                                           USpaceGameLevelConfig const& resources)
    -> std::expected<ioj::settings::Settings, FString>;

[[nodiscard]] SPACEGAME_API auto prepare_level_resources(ioj::settings::Settings const& settings,
                                                         USpaceGameLevelConfig const& resources,
                                                         UObject* outer)
    -> std::expected<USpaceGameLevelConfig*, FString>;
