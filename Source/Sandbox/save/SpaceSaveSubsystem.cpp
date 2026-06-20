#include "SpaceSaveSubsystem.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/save/SpaceSaveGame.h>

#include <CoreMinimal.h>
#include <Kismet/GameplayStatics.h>
#include <Subsystems/GameInstanceSubsystem.h>

void USpaceSaveSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    load_or_create();

    UE_LOG(LogSandboxSubsystem, Display, TEXT("Initialised USpaceSaveSubsystem."));
}

void USpaceSaveSubsystem::Deinitialize() {
    if (current_save) { save_to_disk(); }

    UE_LOG(LogSandboxSubsystem, Display, TEXT("Deinitialising USpaceSaveSubsystem."));

    Super::Deinitialize();
}

auto USpaceSaveSubsystem::get_save() const -> USpaceSaveGame const* {
    return current_save;
}

auto USpaceSaveSubsystem::get_mutable_save() -> USpaceSaveGame& {
    if (!current_save) { load_or_create(); }

    check(current_save);
    return *current_save;
}

void USpaceSaveSubsystem::append_score_record(FScoreRecord const& record) {
    auto& save{get_mutable_save()};

    save.score_records.Add(record);

    save_to_disk();
}

bool USpaceSaveSubsystem::save_to_disk() {
    if (!current_save) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("USpaceSaveSubsystem::save_to_disk: No save to write to."));
        return false;
    }

    auto const saving_slot{slot_name()};

    UE_LOG(LogSandboxSubsystem, Display, TEXT("Saving game: %s"), *saving_slot);

    return UGameplayStatics::SaveGameToSlot(current_save, saving_slot, user_index);
}

bool USpaceSaveSubsystem::load_or_create() {
    auto const loaded_slot_name{slot_name()};

    if (UGameplayStatics::DoesSaveGameExist(loaded_slot_name, user_index)) {
        auto* loaded_save =
            Cast<USpaceSaveGame>(UGameplayStatics::LoadGameFromSlot(loaded_slot_name, user_index));

        if (loaded_save) {
            current_save = loaded_save;
            migrate_if_needed();

            UE_LOG(LogSandboxSubsystem, Display, TEXT("Loaded save file: %s"), *loaded_slot_name);

            return true;
        }

        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("Save file existed but failed to load. Creating a new save."));
    }

    auto* new_save =
        Cast<USpaceSaveGame>(UGameplayStatics::CreateSaveGameObject(USpaceSaveGame::StaticClass()));

    check(new_save);

    current_save = new_save;
    current_save->save_version = USpaceSaveGame::current_save_version;

    UE_LOG(LogSandboxSubsystem, Display, TEXT("Created new save file."));

    return save_to_disk();
}

auto USpaceSaveSubsystem::slot_name() -> FString {
    return TEXT("SandboxSave");
}

void USpaceSaveSubsystem::migrate_if_needed() {
    if (!current_save) { return; }

    if (current_save->save_version < USpaceSaveGame::current_save_version) {
        // Future migrations go here.
        //
        // Example:
        // if (current_save->save_version < 2)
        // {
        //     // Fill defaults for newly-added fields.
        // }

        current_save->save_version = USpaceSaveGame::current_save_version;
        save_to_disk();
    }
}
