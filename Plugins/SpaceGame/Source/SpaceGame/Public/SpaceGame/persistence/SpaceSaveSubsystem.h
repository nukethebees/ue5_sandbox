#pragma once

#include "SpaceGame/persistence/SaveProfileManager.h"

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "SpaceSaveSubsystem.generated.h"

struct FScoreRecord;

namespace ml::ioj {
enum class ELevelProgressState : uint8 {
    NotAttempted,
    Attempted,
    Completed,
};

struct SPACEGAME_API FLevelProgressSummary {
    ELevelProgressState state{ELevelProgressState::NotAttempted};
    int32 attempt_count{};
    int32 completion_count{};
    FDateTime last_played_at{};
    float best_completion_time_seconds{-1.0f};
    int32 best_kills{};
};

SPACEGAME_API auto summarize_level_progress(FName level_name, TConstArrayView<FScoreRecord> records)
    -> FLevelProgressSummary;
}

UCLASS()
class SPACEGAME_API USpaceSaveSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    [[nodiscard]] auto get_profiles() const -> TConstArrayView<FSaveProfileMetadata>;
    [[nodiscard]] auto get_active_profile_id() const -> FString const&;
    [[nodiscard]] auto get_level_progress(FName level_name) const -> ml::ioj::FLevelProgressSummary;
    bool load_profile_records(FString const& profile_id, TArray<FScoreRecord>& records) const;

    auto create_profile(FString display_name) -> ml::ioj::FCreateSaveProfileResponse;
    bool activate_profile(FString const& profile_id);
    bool reset_test_profile();

    [[nodiscard]] auto save_score_record(FScoreRecord const& record) -> bool;
    void log_save_data() const;
  private:
    static constexpr int32 user_index{0};

    static auto profile_index_slot_name() -> FString;
    static auto profile_results_slot_name(FString const& profile_id) -> FString;
    static auto legacy_slot_name() -> FString;

    auto make_storage() -> ml::ioj::FSaveProfileStorage;
    static void migrate_legacy_records(int32 save_version, TArray<FScoreRecord>& records);

    ml::ioj::FSaveProfileManager profile_manager_{};
};
