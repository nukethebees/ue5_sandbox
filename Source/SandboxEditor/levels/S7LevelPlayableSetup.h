#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"
#include <ioj/sim/levels/level_sim_init_data.h>

#include <CoreMinimal.h>

class AS7LevelAuthoringDocument;
class ULevel;
class UWorld;

namespace ioj {
class USpaceGameLevelConfig;
class ATestSpaceShip;
}

namespace ioj::levels {
class LevelDefinition;
}

namespace ioj::editor {
SANDBOXEDITOR_API void prepare_s7_scene_for_pie(UWorld& world);
auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<FString>;
SANDBOXEDITOR_API auto compile_playable_s7_definition(levels::LevelDefinition const& definition,
                                                      USpaceGameLevelConfig const& resources)
    -> S7AuthoringResult<sim::LevelSimInitData>;
}
