#include "SpaceGame/persistence/SpaceSaveSubsystem.h"

#include "TestSaveProfileSource.h"

#include "SpaceGame/persistence/SpaceSaveGame.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <SandboxGameShared/core/SandboxDeveloperSettings.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <HAL/IConsoleManager.h>
#include <Kismet/GameplayStatics.h>

namespace space_save_subsystem {
void reset_test_profile_command(TArray<FString> const&, UWorld* const world) {
    if (!IsValid(world)) {
        UE_LOG(
            LogSandboxSubsystem, Warning, TEXT("spacegame_reset_test_profile: World is invalid."));
        return;
    }

    auto* const game_instance{world->GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("spacegame_reset_test_profile: Save subsystem is invalid."));
        return;
    }

    if (subsystem->reset_test_profile()) {
        UE_LOG(LogSandboxSubsystem,
               Display,
               TEXT("Reset and activated the deterministic Test Profile."));
    }
}

#if !UE_BUILD_SHIPPING
FAutoConsoleCommandWithWorldAndArgs reset_test_profile_console_command{
    TEXT("spacegame_reset_test_profile"),
    TEXT("Replaces the dedicated Test Profile with deterministic outcomes and activates it."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&reset_test_profile_command)};
#endif
}

auto ml::ioj::summarize_level_progress(FName const level_name,
                                       TConstArrayView<FScoreRecord> const records)
    -> FLevelProgressSummary {
    FLevelProgressSummary summary;
    for (auto const& record : records) {
        if (record.level_name != level_name) {
            continue;
        }

        ++summary.attempt_count;
        summary.best_kills = FMath::Max(summary.best_kills, record.kills);
        summary.last_played_at = FMath::Max(summary.last_played_at, record.date);
        if (record.end_state != ETestMissionState::Succeeded) {
            continue;
        }

        ++summary.completion_count;
        if (summary.best_completion_time_seconds < 0.0f ||
            record.time_seconds < summary.best_completion_time_seconds) {
            summary.best_completion_time_seconds = record.time_seconds;
        }
    }

    summary.state = summary.completion_count > 0 ? ELevelProgressState::Completed
                  : summary.attempt_count > 0    ? ELevelProgressState::Attempted
                                                 : ELevelProgressState::NotAttempted;
    return summary;
}

void USpaceSaveSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::Initialize."));

    profile_manager_ = ml::ioj::FSaveProfileManager{make_storage()};
    if (!profile_manager_.initialise()) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("USpaceSaveSubsystem::Initialize: Failed to initialise save profiles."));
        return;
    }

    log_save_data();
}

void USpaceSaveSubsystem::Deinitialize() {
    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::Deinitialize."));
    log_save_data();
    Super::Deinitialize();
}

auto USpaceSaveSubsystem::get_profiles() const -> TConstArrayView<FSaveProfileMetadata> {
    return profile_manager_.get_profiles();
}

auto USpaceSaveSubsystem::get_active_profile_id() const -> FString const& {
    return profile_manager_.get_active_profile_id();
}

auto USpaceSaveSubsystem::get_level_progress(FName const level_name) const
    -> ml::ioj::FLevelProgressSummary {
    return ml::ioj::summarize_level_progress(level_name, profile_manager_.get_active_records());
}

bool USpaceSaveSubsystem::load_profile_records(FString const& profile_id,
                                               TArray<FScoreRecord>& records) const {
    return profile_manager_.load_profile_records(profile_id, records);
}

auto USpaceSaveSubsystem::create_profile(FString display_name)
    -> ml::ioj::FCreateSaveProfileResponse {
    return profile_manager_.create_profile(MoveTemp(display_name));
}

bool USpaceSaveSubsystem::activate_profile(FString const& profile_id) {
    return profile_manager_.activate_profile(profile_id);
}

bool USpaceSaveSubsystem::reset_test_profile() {
#if UE_BUILD_SHIPPING
    return false;
#else
    return profile_manager_.reset_test_profile(ml::ioj::detail::make_test_profile_records());
#endif
}

void USpaceSaveSubsystem::save_score_record(FScoreRecord const& record) {
    if (!profile_manager_.append_score_record(record)) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("USpaceSaveSubsystem::save_score_record: Failed to save mission result."));
    }
}

void USpaceSaveSubsystem::log_save_data() const {
#if WITH_EDITOR
    auto const* const settings{GetDefault<USandboxDeveloperSettings>()};
    if (!settings->print_save_data) {
        return;
    }
#endif

    FString message{FString::Printf(TEXT("\nActive save profile: %s"),
                                    *profile_manager_.get_active_profile_id())};
    auto const profiles{profile_manager_.get_profiles()};
    message += FString::Printf(TEXT("\nProfiles: %d"), profiles.Num());
    for (auto const& profile : profiles) {
        message += FString::Printf(TEXT("\n    %s (%s): %d outcomes, %d kills, %.2f seconds"),
                                   *profile.display_name,
                                   *profile.profile_id,
                                   profile.outcome_count,
                                   profile.total_kills,
                                   profile.total_simulation_duration_seconds);
    }

    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::log_save_data: %s"), *message);
}

auto USpaceSaveSubsystem::profile_index_slot_name() -> FString {
    return TEXT("SpaceProfileIndex");
}

auto USpaceSaveSubsystem::profile_results_slot_name(FString const& profile_id) -> FString {
    return FString::Printf(TEXT("SpaceProfile_%s_Results"), *profile_id);
}

auto USpaceSaveSubsystem::legacy_slot_name() -> FString {
    return TEXT("SandboxSave");
}

auto USpaceSaveSubsystem::make_storage() -> ml::ioj::FSaveProfileStorage {
    return {
        .load_index =
            [](FSaveProfileIndexData& data) {
                auto const slot{profile_index_slot_name()};
                if (!UGameplayStatics::DoesSaveGameExist(slot, user_index)) {
                    return ml::ioj::ESaveProfileLoadResult::not_found;
                }
                auto* const save{Cast<USpaceSaveProfileIndexSaveGame>(
                    UGameplayStatics::LoadGameFromSlot(slot, user_index))};
                if (!IsValid(save)) {
                    return ml::ioj::ESaveProfileLoadResult::failed;
                }
                data = save->data;
                return ml::ioj::ESaveProfileLoadResult::succeeded;
            },
        .save_index =
            [](FSaveProfileIndexData const& data) {
                auto* const save{
                    Cast<USpaceSaveProfileIndexSaveGame>(UGameplayStatics::CreateSaveGameObject(
                        USpaceSaveProfileIndexSaveGame::StaticClass()))};
                if (!IsValid(save)) {
                    return false;
                }
                save->data = data;
                return UGameplayStatics::SaveGameToSlot(
                    save, profile_index_slot_name(), user_index);
            },
        .load_results =
            [](FString const& profile_id, FSaveProfileResultsData& data) {
                auto const slot{profile_results_slot_name(profile_id)};
                if (!UGameplayStatics::DoesSaveGameExist(slot, user_index)) {
                    return ml::ioj::ESaveProfileLoadResult::not_found;
                }
                auto* const save{Cast<USpaceSaveProfileResultsSaveGame>(
                    UGameplayStatics::LoadGameFromSlot(slot, user_index))};
                if (!IsValid(save)) {
                    return ml::ioj::ESaveProfileLoadResult::failed;
                }
                data = save->data;
                return ml::ioj::ESaveProfileLoadResult::succeeded;
            },
        .save_results =
            [](FString const& profile_id, FSaveProfileResultsData const& data) {
                auto* const save{
                    Cast<USpaceSaveProfileResultsSaveGame>(UGameplayStatics::CreateSaveGameObject(
                        USpaceSaveProfileResultsSaveGame::StaticClass()))};
                if (!IsValid(save)) {
                    return false;
                }
                save->data = data;
                return UGameplayStatics::SaveGameToSlot(
                    save, profile_results_slot_name(profile_id), user_index);
            },
        .load_legacy_results =
            [](TArray<FScoreRecord>& records) {
                auto const slot{legacy_slot_name()};
                if (!UGameplayStatics::DoesSaveGameExist(slot, user_index)) {
                    return ml::ioj::ESaveProfileLoadResult::not_found;
                }
                auto* const save{
                    Cast<USpaceSaveGame>(UGameplayStatics::LoadGameFromSlot(slot, user_index))};
                if (!IsValid(save)) {
                    return ml::ioj::ESaveProfileLoadResult::failed;
                }
                records = save->score_records;
                migrate_legacy_records(save->save_version, records);
                return ml::ioj::ESaveProfileLoadResult::succeeded;
            },
    };
}

void USpaceSaveSubsystem::migrate_legacy_records(int32 save_version,
                                                 TArray<FScoreRecord>& records) {
    while (save_version < USpaceSaveGame::current_save_version) {
        switch (save_version) {
            case 1: {
                for (auto& record : records) {
                    record.fail_reason = ETestMissionFailReason::None;
                    if (record.end_state != ETestMissionState::Failed) {
                        continue;
                    }
                    if (record.mission_mode == ETestMissionMode::KillEnemiesWithinTime &&
                        record.time_seconds >= record.target_completion_time) {
                        record.fail_reason = ETestMissionFailReason::TimeElapsed;
                    }
                }
                save_version = 2;
                break;
            }
            default: {
                UE_LOG(LogSandboxSubsystem,
                       Error,
                       TEXT("Unsupported legacy save version %d."),
                       save_version);
                return;
            }
        }
    }
}
