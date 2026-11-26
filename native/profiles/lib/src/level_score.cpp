#include <ioj/profiles/level_score.h>

namespace ioj::profiles {
auto make_level_score(CompletedLevel const& level, Timestamp date) -> std::optional<ScoreRecord> {
    if (!level.player_team || level.reason == sim::RunEndReason::WorldEnd ||
        level.reason == sim::RunEndReason::OrchestratorReset) {
        return std::nullopt;
    }
    return ScoreRecord{.date = date,
                       .level_name = level.metadata.id.value,
                       .team = *level.player_team,
                       .outcome = level.outcome,
                       .end_reason = level.reason,
                       .kills = level.player_team_kills,
                       .time_seconds = static_cast<float>(level.elapsed_time.seconds())};
}
}
