#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"

#include <CoreMinimal.h>
#include <HAL/PlatformProcess.h>

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
SANDBOXEDITOR_API auto prepare_staged_level(AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<FString>;
SANDBOXEDITOR_API auto launch_staged_level(FString const& level, bool exit_when_ready = false)
    -> S7AuthoringResult<FProcHandle>;
SANDBOXEDITOR_API auto refresh_staged_player() -> FProcHandle;
auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<FString>;
SANDBOXEDITOR_API auto validate_playable_s7_definition(levels::LevelDefinition const& definition,
                                                       USpaceGameLevelConfig const& resources)
    -> S7AuthoringResult<void>;
}
