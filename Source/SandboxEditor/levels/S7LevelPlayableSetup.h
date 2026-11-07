#pragma once

#include <ioj/sim/levels/level_sim_init_data.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;
class UWorld;
class USpaceGameLevelConfig;
class ATestSpaceShip;

namespace ioj::levels {
class LevelDefinition;
}

namespace ioj::editor {
SANDBOXEDITOR_API auto launch_s7_level_for_pie(UWorld& world,
                                               levels::LevelDefinition definition,
                                               sim::LevelSimInitData data)
    -> std::expected<void, FString>;
SANDBOXEDITOR_API auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString>;
SANDBOXEDITOR_API auto validate_playable_s7_level(ULevel& level,
                                                  AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString>;
SANDBOXEDITOR_API auto compile_playable_s7_definition(levels::LevelDefinition const& definition,
                                                      USpaceGameLevelConfig const& resources)
    -> std::expected<sim::LevelSimInitData, FString>;
}
