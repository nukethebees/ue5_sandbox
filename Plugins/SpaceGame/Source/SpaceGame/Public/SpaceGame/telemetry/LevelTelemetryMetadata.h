#pragma once

#include <SpaceGame/telemetry/LevelTelemetryRunRecord.h>

class UWorld;
struct FLevelMissionDefinition;

SPACEGAME_API auto
    make_level_telemetry_run_metadata(UWorld const& world,
                                      FLevelMissionDefinition const& mission_definition,
                                      bool presentation_enabled) -> FLevelTelemetryRunMetadata;
