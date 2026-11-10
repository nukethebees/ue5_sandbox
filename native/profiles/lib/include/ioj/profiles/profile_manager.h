#pragma once

#include <ioj/profiles/create_profile_response.h>
#include <ioj/profiles/profile_storage.h>
#include <ioj/profiles/record_save_result.h>

#include <optional>
#include <span>

namespace ioj::profiles {
class ProfileManager {
  public:
    static constexpr std::int32_t max_profile_name_length{32};

    ProfileManager() = default;
    explicit ProfileManager(ProfileStorage storage);

    bool initialise();
    auto profiles() const -> std::span<ProfileMetadata const>;
    auto active_profile_id() const -> std::string const&;
    auto active_records() const -> std::span<ScoreRecord const>;
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
    bool initialise_locked();
    bool reload_latest(std::string const& active_profile_id);
    bool create_initial_profile();
    bool save_new_profile(ProfileMetadata const& metadata,
                          ProfileResultsData results,
                          bool replace_existing);
    auto find_profile(std::string const& profile_id) -> ProfileMetadata*;
    auto find_profile(std::string const& profile_id) const -> ProfileMetadata const*;

    ProfileStorage storage_{};
    ProfileIndexData index_{};
    std::optional<ProfileResultsData> active_results_;
};
}
