#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ioj::persistence {
// UTC seconds since the Unix epoch.
using Timestamp = std::int64_t;

struct ScoreRecord {
    auto operator==(ScoreRecord const&) const -> bool = default;

    Timestamp date{};
    std::string level_name{};
    Team team{Team::White};
    TeamOutcome outcome{TeamOutcome::Defeat};
    std::string objective_id{};
    ObjectiveOutcome objective_outcome{ObjectiveOutcome::Failed};
    std::int32_t kills{};
    float time_seconds{};
};

struct ProfileDebugSettings {
    auto operator==(ProfileDebugSettings const&) const -> bool = default;

    bool unlock_all_missions{};
    bool start_levels_paused{};
};

struct ProfileMetadata {
    auto operator==(ProfileMetadata const&) const -> bool = default;

    std::string profile_id{};
    std::string display_name{};
    Timestamp created_at{};
    Timestamp last_played_at{};
    float total_simulation_duration_seconds{};
    std::int32_t total_kills{};
    std::int32_t outcome_count{};
    ProfileDebugSettings debug_settings{};
};

struct ProfileIndexData {
    static constexpr std::int32_t current_save_version{1};

    auto operator==(ProfileIndexData const&) const -> bool = default;

    std::int32_t save_version{current_save_version};
    std::string active_profile_id{};
    std::vector<ProfileMetadata> profiles{};
};

struct ProfileResultsData {
    static constexpr std::int32_t current_save_version{1};

    auto operator==(ProfileResultsData const&) const -> bool = default;

    std::int32_t save_version{current_save_version};
    std::vector<ScoreRecord> score_records{};
};

auto current_timestamp() -> Timestamp;
auto make_profile_id() -> std::string;
auto sample_profile_records() -> std::vector<ScoreRecord>;
void update_profile_metadata(ProfileMetadata& metadata, std::span<ScoreRecord const> records);
}
