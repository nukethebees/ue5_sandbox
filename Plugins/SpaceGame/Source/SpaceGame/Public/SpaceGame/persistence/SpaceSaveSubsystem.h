#pragma once
#include <ioj/persistence/profile_manager.h>
#include <ioj/persistence/progression.h>
#include <SpaceGameSimulation/levels/LevelTypes.h>

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "SpaceSaveSubsystem.generated.h"

UCLASS()
class SPACEGAME_API USpaceSaveSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
  public:
    void Initialize(FSubsystemCollectionBase& collection) override;
    void Deinitialize() override;
    static auto get(UGameInstance const* game_instance) -> USpaceSaveSubsystem*;
    auto profiles() const -> ioj::persistence::ProfileManager const& { return profile_manager_; }
    auto profiles() -> ioj::persistence::ProfileManager& { return profile_manager_; }
    auto get_active_profile_id() const -> std::string const&;
    auto get_level_progress(ioj::FLevelId level_id) const -> ioj::persistence::LevelProgressSummary;
    auto is_level_completed(ioj::FLevelId level_id) const -> bool;
    auto has_active_profile() const -> bool;
    auto unlock_all_missions() const -> bool;
    auto start_levels_paused() const -> bool;
    auto create_profile(FString const& display_name) -> ioj::persistence::CreateProfileResponse;
    bool activate_profile(std::string const& profile_id);
    bool reset_test_profile();
    bool set_unlock_all_missions(bool enabled);
    bool set_start_levels_paused(bool enabled);
    void log_save_data() const;
  private:
    auto make_storage() -> ioj::persistence::ProfileStorage;
    ioj::persistence::ProfileManager profile_manager_{};
};
