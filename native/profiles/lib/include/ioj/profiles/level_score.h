#pragma once
#include <ioj/duration.h>
#include <ioj/kill_count.h>
#include <ioj/levels/level_metadata.h>
#include <ioj/profiles/score_record.h>

#include <optional>
namespace ioj::sim {
struct LevelSim;
}
namespace ioj::profiles {
struct CompletedLevel {
    levels::LevelMetadata metadata;
    sim::RunEndReason reason;
    std::optional<Team> winner;
    std::optional<Team> player_team;
    LevelOutcome outcome;
    Duration elapsed_time;
    KillCount player_team_kills;
};

auto completed_level(sim::LevelSim const& simulation) -> std::optional<CompletedLevel>;
auto make_level_score(CompletedLevel const& level, Timestamp date) -> std::optional<ScoreRecord>;
}
