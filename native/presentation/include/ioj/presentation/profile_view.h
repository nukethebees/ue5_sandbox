#pragma once

#include <ioj/profiles/level_outcome_summary.h>
#include <ioj/profiles/profile_summary.h>

#include <optional>
#include <span>
#include <string_view>

namespace ioj::presentation {
struct ProfileReportView {
    std::string_view profile_id;
    std::span<profiles::LevelOutcomeSummary const> outcomes;
};

struct ProfileView {
    std::string_view active_profile_id;
    std::span<profiles::ProfileSummary const> summaries;
    std::optional<ProfileReportView> report;
};
}
