#pragma once

#include <ioj/levels/team.h>
#include <ioj/sim/missions/mission_fail_reason.h>
#include <ioj/sim/missions/mission_mode.h>
#include <ioj/sim/missions/mission_state.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>
#include <string>

namespace ioj::sim {
struct LevelTelemetryRunCompletion {
    LevelTelemetryRunEndReason reason{LevelTelemetryRunEndReason::WorldEnd};
    bool interrupted{true};
    std::string completed_utc{};
    std::string world_end_reason{};
    std::optional<MissionMode> mission_mode{};
    std::optional<MissionState> mission_state{};
    std::optional<MissionFailReason> mission_fail_reason{};
    std::optional<double> mission_elapsed_seconds{};
    SimTick completed_ticks{};
    double simulated_elapsed_seconds{};
    std::optional<Team> winning_team{};
};
}
