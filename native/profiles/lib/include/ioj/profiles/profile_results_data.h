#pragma once

#include <ioj/profiles/save_version.h>
#include <ioj/profiles/score_record.h>

#include <vector>

namespace ioj::profiles {
struct ProfileResultsData {
    auto operator==(ProfileResultsData const&) const -> bool = default;

    std::int32_t save_version{current_save_version};
    std::vector<ScoreRecord> score_records{};
};
}
