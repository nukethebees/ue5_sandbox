#pragma once

#include <ioj/sim/telemetry/report.h>

#include <HAL/Platform.h>

class UWorld;
struct FLevelMissionDefinition;

SPACEGAME_API auto
    make_level_telemetry_run_metadata(UWorld const& world,
                                      FLevelMissionDefinition const& mission_definition)
        -> ::ioj::sim::LevelTelemetryRunMetadata;
