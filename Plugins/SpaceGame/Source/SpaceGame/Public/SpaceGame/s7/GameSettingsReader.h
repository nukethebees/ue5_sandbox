#pragma once

#include <ioj/sim/levels/simulation_settings.h>

#include <CoreMinimal.h>

#include <expected>

class USpaceGameLevelConfig;

namespace ioj::levels {
class LevelDefinition;
}

struct FLoadedGameSettings {
    USpaceGameLevelConfig* config{};
    ioj::sim::SimulationSettings simulation;
    FString source;
};

// Require valid resources and an outer that owns the resolved runtime configuration.
[[nodiscard]] SPACEGAME_API auto read_game_settings(FStringView path,
                                                    ioj::levels::LevelDefinition const* level,
                                                    USpaceGameLevelConfig const& resources,
                                                    UObject* outer)
    -> std::expected<FLoadedGameSettings, FString>;
