#pragma once

#include <ioj/profiles/profile_debug_settings.h>
#include <ioj/profiles/score_record.h>

#include <span>
#include <string>

namespace ioj::profiles {
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

void update_profile_metadata(ProfileMetadata& metadata, std::span<ScoreRecord const> records);
}
