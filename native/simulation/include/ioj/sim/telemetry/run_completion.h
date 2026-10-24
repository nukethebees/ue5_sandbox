#pragma once
#include <ioj/duration.h>
#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <optional>
#include <string>

namespace ioj::sim {
struct LevelTelemetryRunCompletion {
    LevelTelemetryRunEndReason reason{LevelTelemetryRunEndReason::WorldEnd};
    std::string completed_utc{};
    std::optional<Team> mission_team{};
    std::optional<TeamOutcome> mission_outcome{};
    std::string decisive_objective_id{};
    std::optional<ObjectiveOutcome> objective_outcome{};
    std::optional<Duration> mission_elapsed_seconds{};
    SimTick completed_ticks{};
    std::optional<Team> winning_team{};
};
}
