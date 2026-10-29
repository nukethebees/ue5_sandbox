#pragma once

#include <ioj/sim/telemetry/report.h>

#include <CoreMinimal.h>

class UGameInstance;

namespace ioj::sim {
struct LevelSim;
}

namespace ioj {
struct FLevelLaunchOptions;

SPACEGAME_API auto take_finalized_level_telemetry_report(sim::LevelSim& simulation,
                                                         FLevelLaunchOptions const& launch_options,
                                                         bool presentation_enabled)
    -> TOptional<sim::LevelTelemetryReport>;

// Leave pending runs unconsumed during automation tests.
SPACEGAME_API void persist_finalized_level_telemetry_run(sim::LevelSim& simulation,
                                                         FLevelLaunchOptions const& launch_options,
                                                         bool presentation_enabled,
                                                         UGameInstance* game_instance);
}
