#pragma once

#include <ioj/sim/telemetry/report.h>

#include <CoreMinimal.h>

#include <expected>
#include <string>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::levels {
struct LaunchOptions;
}

namespace ioj {

SPACEGAME_API auto make_level_telemetry_report(sim::LevelSim const& simulation,
                                               ::ioj::levels::LaunchOptions const& launch_options,
                                               bool presentation_enabled)
    -> TOptional<sim::telemetry::Report>;

SPACEGAME_API auto persist_level_telemetry_run(sim::LevelSim const& simulation,
                                               ::ioj::levels::LaunchOptions const& launch_options,
                                               bool presentation_enabled)
    -> std::expected<std::string, std::string>;
}
