#pragma once

#include <ioj/profiles/unlock_criterion_status.h>

#include <algorithm>
#include <vector>

namespace ioj::profiles {
struct UnlockStatus {
    auto unlocked(bool const ignore_prerequisites = false) const -> bool {
        return ignore_prerequisites ||
               std::ranges::all_of(criteria, &UnlockCriterionStatus::satisfied);
    }

    std::vector<UnlockCriterionStatus> criteria{};
};
}
