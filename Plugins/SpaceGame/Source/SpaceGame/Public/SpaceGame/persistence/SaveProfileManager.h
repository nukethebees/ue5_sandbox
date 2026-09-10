#pragma once

#include "SpaceGame/persistence/SpaceSaveGame.h"

#include <Containers/ArrayView.h>
#include <Misc/Optional.h>
#include <Templates/Function.h>

namespace ml::ioj {
enum class ESaveProfileLoadResult : uint8 {
    succeeded,
    not_found,
    failed,
};

enum class ECreateSaveProfileResult : uint8 {
    succeeded,
    empty_name,
    name_too_long,
    duplicate_name,
    persistence_failed,
};

struct SPACEGAME_API FCreateSaveProfileResponse {
    ECreateSaveProfileResult result{ECreateSaveProfileResult::persistence_failed};
    FString profile_id{};
};

struct SPACEGAME_API FSaveProfileStorage {
    TFunction<ESaveProfileLoadResult(FSaveProfileIndexData&)> load_index{};
    TFunction<bool(FSaveProfileIndexData const&)> save_index{};
    TFunction<ESaveProfileLoadResult(FString const&, FSaveProfileResultsData&)> load_results{};
    TFunction<bool(FString const&, FSaveProfileResultsData const&)> save_results{};
    TFunction<ESaveProfileLoadResult(TArray<FScoreRecord>&)> load_legacy_results{};
};

class SPACEGAME_API FSaveProfileManager {
  public:
    static constexpr int32 max_profile_name_length{32};

    /* **************************************** */
    // Lifecycle and queries
    /* **************************************** */
    FSaveProfileManager() = default;
    explicit FSaveProfileManager(FSaveProfileStorage storage);

    bool initialise();

    [[nodiscard]] auto get_profiles() const -> TConstArrayView<FSaveProfileMetadata>;
    [[nodiscard]] auto get_active_profile_id() const -> FString const&;
    [[nodiscard]] auto get_active_records() const -> TConstArrayView<FScoreRecord>;
    [[nodiscard]] auto has_active_profile() const -> bool;
    [[nodiscard]] auto unlock_all_missions() const -> bool;
    [[nodiscard]] auto start_levels_paused() const -> bool;

    /* **************************************** */
    // Profile and result mutation
    /* **************************************** */
    auto create_profile(FString display_name) -> FCreateSaveProfileResponse;
    bool activate_profile(FString const& profile_id);
    bool load_profile_records(FString const& profile_id, TArray<FScoreRecord>& records) const;
    bool append_score_record(FScoreRecord const& record);
    bool set_unlock_all_missions(bool enabled);
    bool set_start_levels_paused(bool enabled);
    bool reset_test_profile(TConstArrayView<FScoreRecord> records);

    /* **************************************** */
    // Validation
    /* **************************************** */
    [[nodiscard]] static auto validate_profile_name(FString display_name,
                                                    TConstArrayView<FSaveProfileMetadata> profiles)
        -> ECreateSaveProfileResult;
  private:
    /* **************************************** */
    // Persistence and migration
    /* **************************************** */
    static auto make_metadata(FString profile_id,
                              FString display_name,
                              FDateTime created_at,
                              TConstArrayView<FScoreRecord> records) -> FSaveProfileMetadata;
    static void update_metadata(FSaveProfileMetadata& metadata,
                                TConstArrayView<FScoreRecord> records);
    static auto migrate_index(FSaveProfileIndexData& index) -> bool;
    static auto results_version_is_supported(FSaveProfileResultsData const& results) -> bool;

    bool create_initial_profile(TConstArrayView<FScoreRecord> legacy_records);
    bool save_new_profile(FSaveProfileMetadata metadata,
                          FSaveProfileResultsData results,
                          bool replace_existing);
    auto find_profile(FString const& profile_id) -> FSaveProfileMetadata*;
    auto find_profile(FString const& profile_id) const -> FSaveProfileMetadata const*;

    /* **************************************** */
    // State
    /* **************************************** */
    FSaveProfileStorage storage_{};
    FSaveProfileIndexData index_{};
    FSaveProfileResultsData active_results_{};
    bool initialised_{};
};
}
