#include "SpaceSaveSubsystem.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/save/SpaceSaveGame.h>
#include <Sandbox/utilities/enums.h>

#include <CoreMinimal.h>
#include <Kismet/GameplayStatics.h>
#include <Subsystems/GameInstanceSubsystem.h>

void USpaceSaveSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::Initialize."));

    load_or_create();
    log_save_data();
}
void USpaceSaveSubsystem::Deinitialize() {
    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::Deinitialize."));

    if (current_save) { save_to_disk(); }

    log_save_data();

    Super::Deinitialize();
}

// Accessors
auto USpaceSaveSubsystem::get_save() const -> USpaceSaveGame const* {
    return current_save;
}
auto USpaceSaveSubsystem::get_mutable_save() -> USpaceSaveGame& {
    if (!current_save) { load_or_create(); }

    check(current_save);
    return *current_save;
}
auto USpaceSaveSubsystem::slot_name() -> FString {
    return TEXT("SandboxSave");
}

// Appending
void USpaceSaveSubsystem::save_score_record(FScoreRecord const& record) {
    auto& save{get_mutable_save()};

    save.score_records.Add(record);

    save_to_disk();
}

// Loading
bool USpaceSaveSubsystem::load_or_create() {
    auto const loaded_slot_name{slot_name()};

    if (UGameplayStatics::DoesSaveGameExist(loaded_slot_name, user_index)) {
        auto* loaded_save =
            Cast<USpaceSaveGame>(UGameplayStatics::LoadGameFromSlot(loaded_slot_name, user_index));

        if (loaded_save) {
            current_save = loaded_save;
            migrate_if_needed();

            UE_LOG(LogSandboxSubsystem,
                   Display,
                   TEXT("USpaceSaveSubsystem::load_or_create: Loaded save file: %s"),
                   *loaded_slot_name);

            return true;
        }

        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("USpaceSaveSubsystem::load_or_create: Save file existed but failed to load. "
                    "Creating a new save."));
    }

    auto* new_save =
        Cast<USpaceSaveGame>(UGameplayStatics::CreateSaveGameObject(USpaceSaveGame::StaticClass()));

    check(new_save);

    current_save = new_save;
    current_save->save_version = USpaceSaveGame::current_save_version;

    UE_LOG(LogSandboxSubsystem,
           Display,
           TEXT("USpaceSaveSubsystem::load_or_create: Created new save file."));

    return save_to_disk();
}

// Saving
bool USpaceSaveSubsystem::save_to_disk() {
    if (!current_save) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("USpaceSaveSubsystem::save_to_disk: No save to write to."));
        return false;
    }

    auto const saving_slot{slot_name()};

    UE_LOG(LogSandboxSubsystem,
           Display,
           TEXT("USpaceSaveSubsystem::save_to_disk: Saving game: %s"),
           *saving_slot);

    return UGameplayStatics::SaveGameToSlot(current_save, saving_slot, user_index);
}

// Displaying
void USpaceSaveSubsystem::log_save_data() const {
    if (!current_save) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("USpaceSaveSubsystem::log_save_data: No save to print."));
        return;
    }

    auto const dump_name{slot_name()};

    FString msg{};
    msg += FString::Printf(TEXT("\nPrinting save slot: %s"), *dump_name);

    auto const n{current_save->score_records.Num()};
    msg += FString::Printf(TEXT("\nRecords: %d"), n);

    for (auto const& record : current_save->score_records) {
        msg += FString::Printf(TEXT(R"(
    Date: %s, Level: %s, Mode: %s, End state: %s
        Kills: %d, Time: %.2f)"),
                               *record.date.ToString(),
                               *record.level_name.ToString(),
                               *ml::to_string_without_type_prefix(record.mission_mode),
                               *ml::to_string_without_type_prefix(record.end_state),
                               record.kills,
                               record.time_seconds);

        switch (record.mission_mode) {
            case ETestMissionMode::None: {
                break;
            }
            case ETestMissionMode::KillEnemies: {
                msg += FString::Printf(TEXT(", Kill target: %d"), record.target_kills);
                break;
            }
            case ETestMissionMode::SurviveTime: {
                msg += FString::Printf(TEXT(", Target completion time: %.2f"),
                                       record.target_completion_time);
                break;
            }
        }

        if (record.end_state == ETestMissionState::Failed) {
            msg += FString::Printf(TEXT("\n    Fail reason: %s"),
                                   *ml::to_string_without_type_prefix(record.fail_reason));
        }
    }

    UE_LOG(LogSandboxSubsystem, Display, TEXT("USpaceSaveSubsystem::log_save_data: %s"), *msg);
}

// Migration
void USpaceSaveSubsystem::migrate_if_needed() {
    if (!current_save) { return; }

    while (current_save->save_version < USpaceSaveGame::current_save_version) {
        switch (current_save->save_version) {
            case 1:
                migrate_to_v2();
                current_save->save_version = 2;
                break;
            default:
                checkNoEntry(); // Unknown or unsupported version.
                return;
        }
    }

    save_to_disk();
}
void USpaceSaveSubsystem::migrate_to_v2() {
    for (auto& record : current_save->score_records) {
        record.fail_reason = ETestMissionFailReason::None;

        if (record.end_state == ETestMissionState::Failed) { continue; }

        switch (record.mission_mode) {
            case ETestMissionMode::KillEnemiesWithinTime: {
                if (record.time_seconds >= record.target_completion_time) {
                    record.fail_reason = ETestMissionFailReason::TimeElapsed;
                }
            }
            default: {
                break;
            }
        }
    }
}
