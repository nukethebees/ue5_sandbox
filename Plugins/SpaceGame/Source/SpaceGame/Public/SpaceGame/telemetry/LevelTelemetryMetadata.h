#pragma once

#include <ioj/sim/telemetry/report.h>

#include <HAL/Platform.h>

class UWorld;
namespace ioj::levels {
struct LevelMetadata;
}

SPACEGAME_API auto make_level_telemetry_run_metadata(UWorld const& world,
                                                     ioj::levels::LevelMetadata const& level)
    -> ::ioj::sim::telemetry::RunMetadata;
