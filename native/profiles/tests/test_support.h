#pragma once

#include <ioj/profiles/profile_storage.h>
#include <ioj/profiles/timestamp.h>

#include <chrono>
#include <unordered_map>

namespace ioj::profiles {
struct FakeProfileStorage {
    bool index_exists{};
    bool index_load_fails{};
    bool index_save_fails{};
    bool results_save_fails{};
    std::int32_t index_save_count{};
    ProfileIndexData index{};
    std::unordered_map<std::string, ProfileResultsData> results{};
    auto make() -> ProfileStorage {
        return {.with_exclusive_access =
                    [](std::function<bool()> const& operation) { return operation(); },
                .load_index =
                    [this](ProfileIndexData& output) {
                        if (index_load_fails) {
                            return LoadResult::failed;
                        }
                        if (!index_exists) {
                            return LoadResult::not_found;
                        }
                        output = index;
                        return LoadResult::succeeded;
                    },
                .save_index =
                    [this](ProfileIndexData const& value) {
                        ++index_save_count;
                        if (index_save_fails) {
                            return false;
                        }
                        index = value;
                        index_exists = true;
                        return true;
                    },
                .load_results =
                    [this](std::string const& id, ProfileResultsData& output) {
                        auto found{results.find(id)};
                        if (found == results.end()) {
                            return LoadResult::not_found;
                        }
                        output = found->second;
                        return LoadResult::succeeded;
                    },
                .save_results =
                    [this](std::string const& id, ProfileResultsData const& value) {
                        if (results_save_fails) {
                            return false;
                        }
                        results.insert_or_assign(id, value);
                        return true;
                    }};
    }
};

inline auto test_date(int year, unsigned month, unsigned day, int hour = 0, int minute = 0)
    -> Timestamp {
    using namespace std::chrono;
    return duration_cast<seconds>(
               sys_days{std::chrono::year{year} / month / day}.time_since_epoch())
               .count() +
           hour * 3600 + minute * 60;
}
}
