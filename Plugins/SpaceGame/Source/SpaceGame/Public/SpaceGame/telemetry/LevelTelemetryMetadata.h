#pragma once

#include <SpaceGame/telemetry/LevelTelemetryReport.h>

class UWorld;
struct FLevelMissionDefinition;

SPACEGAME_API auto
    make_level_telemetry_run_metadata(UWorld const& world,
                                      FLevelMissionDefinition const& mission_definition)
        -> FLevelTelemetryRunMetadata;

SPACEGAME_API auto make_level_telemetry_environment(UWorld const& world)
    -> FLevelTelemetryEnvironment;
