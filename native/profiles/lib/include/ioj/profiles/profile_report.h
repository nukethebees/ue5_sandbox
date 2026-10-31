#pragma once

#include <ioj/profiles/level_outcome_summary.h>

#include <string>
#include <vector>

namespace ioj::profiles {
struct ProfileReport {
    std::string profile_id{};
    std::vector<LevelOutcomeSummary> outcomes{};
};
}
