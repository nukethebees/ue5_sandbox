#pragma once

#include "SpaceGame/persistence/SaveProfileManager.h"

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "SpaceSaveSubsystem.generated.h"

struct FScoreRecord;

UCLASS()
class SPACEGAME_API USpaceSaveSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    [[nodiscard]] auto get_profiles() const -> TConstArrayView<FSaveProfileMetadata>;
    [[nodiscard]] auto get_active_profile_id() const -> FString const&;
    bool load_profile_records(FString const& profile_id, TArray<FScoreRecord>& records) const;

    auto create_profile(FString display_name) -> ml::ioj::FCreateSaveProfileResponse;
    bool activate_profile(FString const& profile_id);
    bool reset_test_profile();

    void save_score_record(FScoreRecord const& record);
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
