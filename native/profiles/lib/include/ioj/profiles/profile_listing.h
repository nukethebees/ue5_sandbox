#pragma once

#include <ioj/profiles/profile_summary.h>

#include <string>
#include <vector>

namespace ioj::profiles {
struct ProfileListing {
    std::string active_profile_id{};
    std::vector<ProfileSummary> summaries{};
};
}
