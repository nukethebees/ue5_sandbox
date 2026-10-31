#pragma once

#include <ioj/profiles/level_outcome_statistic.h>
#include <ioj/profiles/timestamp.h>

#include <vector>

namespace ioj::profiles {
struct LevelOutcomeSummary {
    std::string outcome_id{};
    std::string display_name{};
    Timestamp completed_at{};
    float simulation_duration_seconds{};
    std::int32_t kills{};
    std::string result{};
    std::vector<LevelOutcomeStatistic> statistics{};
};
}
