#pragma once

#include <ioj/profiles/timestamp.h>

#include <string>

namespace ioj::profiles {
struct ProfileSummary {
    std::string profile_id{};
    std::string display_name{};
    Timestamp created_at{};
    Timestamp last_played_at{};
    float total_simulation_duration_seconds{};
    std::int32_t total_kills{};
    std::int32_t outcome_count{};
};
}
