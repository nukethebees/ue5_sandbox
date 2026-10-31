#pragma once

#include <ioj/persistence/profile.h>

#include <functional>
#include <span>

namespace ioj::persistence {
enum class LoadResult : std::uint8_t { succeeded, not_found, failed };
enum class CreateProfileResult : std::uint8_t {
    succeeded,
    empty_name,
    name_too_long,
    duplicate_name,
    persistence_failed
};

struct CreateProfileResponse {
    CreateProfileResult result{CreateProfileResult::persistence_failed};
    std::string profile_id{};
};

struct RecordSaveResult {
    explicit operator bool() const { return persisted; }

    bool persisted{};
    bool new_best_time{};
};

struct ProfileStorage {
    std::function<bool(std::function<bool()> const&)> with_exclusive_access{};
    std::function<LoadResult(ProfileIndexData&)> load_index{};
    std::function<bool(ProfileIndexData const&)> save_index{};
    std::function<LoadResult(std::string const&, ProfileResultsData&)> load_results{};
    std::function<bool(std::string const&, ProfileResultsData const&)> save_results{};
};

class ProfileManager {
  public:
    static constexpr std::int32_t max_profile_name_length{32};

    ProfileManager() = default;
    explicit ProfileManager(ProfileStorage storage);

    bool initialise();
    auto get_profiles() const -> std::span<ProfileMetadata const>;
    auto get_active_profile_id() const -> std::string const&;
    auto get_active_records() const -> std::span<ScoreRecord const>;
    auto has_active_profile() const -> bool;
    auto unlock_all_missions() const -> bool;
    auto start_levels_paused() const -> bool;

    auto create_profile(std::string display_name) -> CreateProfileResponse;
    bool activate_profile(std::string const& profile_id);
    bool load_profile_records(std::string const& profile_id,
                              std::vector<ScoreRecord>& records) const;
    auto append_score_record(ScoreRecord const& record) -> RecordSaveResult;
    bool set_unlock_all_missions(bool enabled);
    bool set_start_levels_paused(bool enabled);
    bool reset_test_profile(std::span<ScoreRecord const> records);

    static auto validate_profile_name(std::string display_name,
                                      std::span<ProfileMetadata const> profiles)
        -> CreateProfileResult;
  private:
    static auto make_metadata(std::string profile_id,
                              std::string display_name,
                              Timestamp created_at,
                              std::span<ScoreRecord const> records) -> ProfileMetadata;
    static auto index_version_is_supported(ProfileIndexData const& index) -> bool;
    static auto results_version_is_supported(ProfileResultsData const& results) -> bool;

    bool initialise_locked();
    bool reload_latest(std::string const& active_profile_id, bool load_active_results);
    bool create_initial_profile();
    bool save_new_profile(ProfileMetadata metadata,
                          ProfileResultsData results,
                          bool replace_existing);
    auto find_profile(std::string const& profile_id) -> ProfileMetadata*;
    auto find_profile(std::string const& profile_id) const -> ProfileMetadata const*;

    ProfileStorage storage_{};
    ProfileIndexData index_{};
    ProfileResultsData active_results_{};
    bool initialised_{};
};
}
