#pragma once

#include <ioj/profiles/record_save_result.h>
#include <ioj/profiles/score_record.h>

#include <optional>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::profiles {
class ProfileManager;

struct CompletionPublication {
    ScoreRecord record{};
    std::string level_title{};
    std::optional<float> par_time_seconds{};
    RecordSaveResult save_result{};
};

// Publish a retained result; the caller must invoke this only for a newly observed completion.
auto publish_player_completion(sim::LevelSim& simulation, ProfileManager* profiles, Timestamp date)
    -> std::optional<CompletionPublication>;
}
