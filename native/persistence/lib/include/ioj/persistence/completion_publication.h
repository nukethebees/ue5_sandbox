#pragma once

#include <ioj/persistence/profile_manager.h>

#include <optional>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::persistence {
struct CompletionPublication {
    ScoreRecord record{};
    std::string level_title{};
    std::optional<float> par_time_seconds{};
    RecordSaveResult save_result{};
};

// Consumes the pending player-team completion even when profiles are unavailable.
auto publish_player_completion(sim::LevelSim& simulation, ProfileManager* profiles, Timestamp date)
    -> std::optional<CompletionPublication>;
}
