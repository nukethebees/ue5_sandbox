#pragma once

#include "SpaceGame/persistence/SaveGameBrowser.h"

class USpaceSaveSubsystem;

namespace ml::ioj::detail {
auto discover_existing_save_profiles(USpaceSaveSubsystem const& save_subsystem)
    -> TArray<FSaveProfileSummary>;
auto load_existing_save_profile(USpaceSaveSubsystem const& save_subsystem,
                                FString const& profile_id) -> TOptional<FSaveProfileReport>;
}
