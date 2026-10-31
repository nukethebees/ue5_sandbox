#pragma once

#include <ioj/profiles/load_result.h>
#include <ioj/profiles/profile_index_data.h>
#include <ioj/profiles/profile_results_data.h>

#include <functional>

namespace ioj::profiles {
struct ProfileStorage {
    std::function<bool(std::function<bool()> const&)> with_exclusive_access{};
    std::function<LoadResult(ProfileIndexData&)> load_index{};
    std::function<bool(ProfileIndexData const&)> save_index{};
    std::function<LoadResult(std::string const&, ProfileResultsData&)> load_results{};
    std::function<bool(std::string const&, ProfileResultsData const&)> save_results{};
};
}
