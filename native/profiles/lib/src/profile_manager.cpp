#include <ioj/profiles/profile_manager.h>

#include <ioj/profiles/profile_id.h>
#include <ioj/profiles/profile_metadata.h>
#include <ioj/profiles/progression.h>
#include <ioj/profiles/timestamp.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <utility>

namespace ioj::profiles {
namespace save_profile_manager {
void trim(std::string& value) {
    auto const first{value.find_first_not_of(" \t\n\r")};
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    value = value.substr(first, value.find_last_not_of(" \t\n\r") - first + 1);
}
auto case_insensitive_equal(std::string_view a, std::string_view b) -> bool {
    return std::ranges::equal(a, b, [](unsigned char left, unsigned char right) {
        return std::tolower(left) == std::tolower(right);
    });
}
inline std::string const default_profile_id{"default_profile"};
inline std::string const default_profile_name{"Default Profile"};
inline std::string const test_profile_id{"test_profile"};
inline std::string const test_profile_name{"Test Profile"};
}

/* **************************************** */
// Lifecycle and queries
/* **************************************** */
ProfileManager::ProfileManager(ProfileStorage storage)
    : storage_{std::move(storage)} {}

bool ProfileManager::initialise() {
    if (!storage_.with_exclusive_access || !storage_.load_index || !storage_.save_index ||
        !storage_.load_results || !storage_.save_results) {
        return false;
    }

    return storage_.with_exclusive_access([this] { return initialise_locked(); });
}

bool ProfileManager::initialise_locked() {
    ProfileIndexData loaded_index;
    auto const index_result{storage_.load_index(loaded_index)};
    if (index_result == LoadResult::failed) {
        return false;
    }
    if (index_result == LoadResult::not_found) {
        return create_initial_profile();
    }
    if (!index_version_is_supported(loaded_index)) {
        return false;
    }
    if (loaded_index.profiles.empty()) {
        return create_initial_profile();
    }
    auto active{std::ranges::find(
        loaded_index.profiles, loaded_index.active_profile_id, &ProfileMetadata::profile_id)};
    if (active == loaded_index.profiles.end()) {
        active = loaded_index.profiles.begin();
        loaded_index.active_profile_id = active->profile_id;
    }
    ProfileResultsData results;
    if (storage_.load_results(loaded_index.active_profile_id, results) != LoadResult::succeeded ||
        !results_version_is_supported(results)) {
        return false;
    }
    update_profile_metadata(*active, results.score_records);
    if (!storage_.save_index(loaded_index)) {
        return false;
    }
    index_ = std::move(loaded_index);
    active_results_ = std::move(results);
    return true;
}

auto ProfileManager::get_profiles() const -> std::span<ProfileMetadata const> {
    return index_.profiles;
}

auto ProfileManager::get_active_profile_id() const -> std::string const& {
    return index_.active_profile_id;
}

auto ProfileManager::get_active_records() const -> std::span<ScoreRecord const> {
    return active_results_ ? std::span<ScoreRecord const>{active_results_->score_records}
                           : std::span<ScoreRecord const>{};
}

auto ProfileManager::has_active_profile() const -> bool {
    return active_results_.has_value() && find_profile(index_.active_profile_id) != nullptr;
}

auto ProfileManager::unlock_all_missions() const -> bool {
    auto const* const profile{find_profile(index_.active_profile_id)};
    return has_active_profile() && profile->debug_settings.unlock_all_missions;
}

auto ProfileManager::start_levels_paused() const -> bool {
    auto const* const profile{find_profile(index_.active_profile_id)};
    return has_active_profile() && profile->debug_settings.start_levels_paused;
}

/* **************************************** */
// Profile and result mutation
/* **************************************** */
auto ProfileManager::create_profile(std::string display_name) -> CreateProfileResponse {
    CreateProfileResponse response{};
    if (!active_results_.has_value()) {
        return response;
    }
    auto const active_profile_id{index_.active_profile_id};
    storage_.with_exclusive_access([this, &display_name, &response, &active_profile_id] {
        if (!reload_latest(active_profile_id)) {
            return false;
        }

        save_profile_manager::trim(display_name);
        auto const validation{validate_profile_name(display_name, index_.profiles)};
        if (validation != CreateProfileResult::succeeded) {
            response.result = validation;
            return true;
        }

        auto const profile_id{make_profile_id()};
        auto metadata{make_metadata(profile_id, display_name, current_timestamp(), {})};
        if (!save_new_profile(std::move(metadata), {}, false)) {
            return false;
        }

        response = {.result = CreateProfileResult::succeeded, .profile_id = profile_id};
        return true;
    });
    return response;
}

bool ProfileManager::activate_profile(std::string const& profile_id) {
    if (!active_results_.has_value() || profile_id == index_.active_profile_id) {
        return active_results_.has_value() && find_profile(profile_id) != nullptr;
    }
    auto const previous_index{index_};
    auto const previous_results{active_results_};
    auto const succeeded{storage_.with_exclusive_access([this, &profile_id] {
        ProfileIndexData latest_index{};
        if (storage_.load_index(latest_index) != LoadResult::succeeded ||
            !index_version_is_supported(latest_index)) {
            return false;
        }
        index_ = std::move(latest_index);
        if (!find_profile(profile_id)) {
            return false;
        }

        ProfileResultsData loaded_results{};
        if (storage_.load_results(profile_id, loaded_results) != LoadResult::succeeded ||
            !results_version_is_supported(loaded_results)) {
            return false;
        }

        index_.active_profile_id = profile_id;
        auto* const metadata{find_profile(profile_id)};
        assert(metadata);
        update_profile_metadata(*metadata, loaded_results.score_records);
        if (!storage_.save_index(index_)) {
            return false;
        }

        active_results_ = std::move(loaded_results);
        return true;
    })};
    if (!succeeded) {
        index_ = previous_index;
        active_results_ = previous_results;
    }
    return succeeded;
}

bool ProfileManager::load_profile_records(std::string const& profile_id,
                                          std::vector<ScoreRecord>& records) const {
    if (!active_results_.has_value() || !find_profile(profile_id)) {
        return false;
    }
    if (profile_id == index_.active_profile_id) {
        records = active_results_->score_records;
        return true;
    }

    return storage_.with_exclusive_access([this, &profile_id, &records] {
        ProfileResultsData results{};
        if (storage_.load_results(profile_id, results) != LoadResult::succeeded ||
            !results_version_is_supported(results)) {
            return false;
        }

        records = std::move(results.score_records);
        return true;
    });
}

auto ProfileManager::append_score_record(ScoreRecord const& record) -> RecordSaveResult {
    if (!active_results_.has_value()) {
        return {};
    }

    auto const active_profile_id{index_.active_profile_id};
    bool new_best_time{};
    auto const persisted{
        storage_.with_exclusive_access([this, &record, &active_profile_id, &new_best_time] {
            if (!reload_latest(active_profile_id)) {
                return false;
            }

            if (record.outcome == MissionOutcome::Victory) {
                auto const progress{summarize_level_progress(levels::LevelId{record.level_name},
                                                             active_results_->score_records)};
                new_best_time = progress.best_completion_time_seconds < 0.f ||
                                record.time_seconds < progress.best_completion_time_seconds;
            }

            active_results_->score_records.push_back(record);
            if (!storage_.save_results(active_profile_id, *active_results_)) {
                active_results_->score_records.pop_back();
                return false;
            }

            auto* const metadata{find_profile(active_profile_id)};
            assert(metadata);
            update_profile_metadata(*metadata, active_results_->score_records);
            // Results are authoritative; initialization repairs stale aggregate metadata.
            static_cast<void>(storage_.save_index(index_));
            return true;
        })};
    return {.persisted = persisted, .new_best_time = persisted && new_best_time};
}

bool ProfileManager::set_unlock_all_missions(bool const enabled) {
    if (!active_results_.has_value()) {
        return false;
    }

    auto const active_profile_id{index_.active_profile_id};
    return storage_.with_exclusive_access([this, enabled, &active_profile_id] {
        if (!reload_latest(active_profile_id)) {
            return false;
        }
        auto* const metadata{find_profile(active_profile_id)};
        if (metadata == nullptr) {
            return false;
        }
        if (metadata->debug_settings.unlock_all_missions == enabled) {
            return true;
        }

        auto const previous{metadata->debug_settings.unlock_all_missions};
        metadata->debug_settings.unlock_all_missions = enabled;
        if (!storage_.save_index(index_)) {
            metadata->debug_settings.unlock_all_missions = previous;
            return false;
        }
        return true;
    });
}

bool ProfileManager::set_start_levels_paused(bool const enabled) {
    if (!active_results_.has_value()) {
        return false;
    }

    auto const active_profile_id{index_.active_profile_id};
    return storage_.with_exclusive_access([this, enabled, &active_profile_id] {
        if (!reload_latest(active_profile_id)) {
            return false;
        }
        auto* const metadata{find_profile(active_profile_id)};
        if (metadata == nullptr) {
            return false;
        }
        if (metadata->debug_settings.start_levels_paused == enabled) {
            return true;
        }

        auto const previous{metadata->debug_settings.start_levels_paused};
        metadata->debug_settings.start_levels_paused = enabled;
        if (!storage_.save_index(index_)) {
            metadata->debug_settings.start_levels_paused = previous;
            return false;
        }
        return true;
    });
}

bool ProfileManager::reset_test_profile(std::span<ScoreRecord const> const records) {
    if (!active_results_.has_value()) {
        return false;
    }
    auto const active_profile_id{index_.active_profile_id};
    return storage_.with_exclusive_access([this, records, &active_profile_id] {
        if (!reload_latest(active_profile_id)) {
            return false;
        }
        auto const created_at{records.empty() ? current_timestamp() : records[0].date};
        auto metadata{make_metadata(save_profile_manager::test_profile_id,
                                    save_profile_manager::test_profile_name,
                                    created_at,
                                    records)};
        ProfileResultsData results{};
        results.score_records.assign(records.begin(), records.end());
        return save_new_profile(std::move(metadata), std::move(results), true);
    });
}

/* **************************************** */
// Validation
/* **************************************** */
auto ProfileManager::validate_profile_name(std::string display_name,
                                           std::span<ProfileMetadata const> const profiles)
    -> CreateProfileResult {
    save_profile_manager::trim(display_name);
    if (display_name.empty()) {
        return CreateProfileResult::empty_name;
    }
    if (display_name.size() > max_profile_name_length) {
        return CreateProfileResult::name_too_long;
    }
    for (auto const& profile : profiles) {
        if (save_profile_manager::case_insensitive_equal(profile.display_name, display_name)) {
            return CreateProfileResult::duplicate_name;
        }
    }
    return CreateProfileResult::succeeded;
}

/* **************************************** */
// Persistence
/* **************************************** */
auto ProfileManager::make_metadata(std::string profile_id,
                                   std::string display_name,
                                   Timestamp const created_at,
                                   std::span<ScoreRecord const> const records) -> ProfileMetadata {
    ProfileMetadata metadata{
        .profile_id = std::move(profile_id),
        .display_name = std::move(display_name),
        .created_at = created_at,
    };
    update_profile_metadata(metadata, records);
    return metadata;
}

auto ProfileManager::index_version_is_supported(ProfileIndexData const& index) -> bool {
    return index.save_version == ProfileIndexData::current_save_version;
}

auto ProfileManager::results_version_is_supported(ProfileResultsData const& results) -> bool {
    return results.save_version == ProfileResultsData::current_save_version;
}

bool ProfileManager::reload_latest(std::string const& active_profile_id) {
    ProfileIndexData latest_index;
    if (storage_.load_index(latest_index) != LoadResult::succeeded ||
        !index_version_is_supported(latest_index) ||
        std::ranges::find(latest_index.profiles, active_profile_id, &ProfileMetadata::profile_id) ==
            latest_index.profiles.end()) {
        return false;
    }
    ProfileResultsData latest_results;
    if (storage_.load_results(active_profile_id, latest_results) != LoadResult::succeeded ||
        !results_version_is_supported(latest_results)) {
        return false;
    }
    latest_index.active_profile_id = active_profile_id;
    index_ = std::move(latest_index);
    active_results_ = std::move(latest_results);
    return true;
}

bool ProfileManager::create_initial_profile() {
    auto metadata{make_metadata(save_profile_manager::default_profile_id,
                                save_profile_manager::default_profile_name,
                                current_timestamp(),
                                {})};
    return save_new_profile(std::move(metadata), {}, false);
}

bool ProfileManager::save_new_profile(ProfileMetadata metadata,
                                      ProfileResultsData results,
                                      bool const replace_existing) {
    if (!storage_.save_results(metadata.profile_id, results)) {
        return false;
    }

    auto const previous_index{index_};
    auto* existing{find_profile(metadata.profile_id)};
    if (existing) {
        if (!replace_existing) {
            return false;
        }
        *existing = metadata;
    } else {
        index_.profiles.push_back(metadata);
    }
    index_.active_profile_id = metadata.profile_id;

    if (!storage_.save_index(index_)) {
        index_ = previous_index;
        return false;
    }

    active_results_ = std::move(results);
    return true;
}

auto ProfileManager::find_profile(std::string const& profile_id) -> ProfileMetadata* {
    auto const found{std::ranges::find(index_.profiles, profile_id, &ProfileMetadata::profile_id)};
    return found == index_.profiles.end() ? nullptr : &*found;
}
auto ProfileManager::find_profile(std::string const& profile_id) const -> ProfileMetadata const* {
    auto const found{std::ranges::find(index_.profiles, profile_id, &ProfileMetadata::profile_id)};
    return found == index_.profiles.end() ? nullptr : &*found;
}
}
