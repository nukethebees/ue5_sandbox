#include "ExistingSaveGameBrowserSource.h"

#include "SpaceGame/persistence/SaveGameProfileAdapter.h"
#include "SpaceGame/persistence/SpaceSaveGame.h"
#include "SpaceGame/persistence/SpaceSaveSubsystem.h"

namespace ml::ioj::detail {
namespace existing_save_game_browser_source {
FString const profile_display_name{TEXT("Sandbox Profile")};
}

auto discover_existing_save_profiles(USpaceSaveSubsystem const& save_subsystem)
    -> TArray<FSaveProfileSummary> {
    auto const* save{save_subsystem.get_save()};
    if (!IsValid(save)) {
        return {};
    }

    return {save_profile::make_summary(USpaceSaveSubsystem::slot_name(),
                                       existing_save_game_browser_source::profile_display_name,
                                       save->score_records)};
}

auto load_existing_save_profile(USpaceSaveSubsystem const& save_subsystem,
                                FString const& profile_id) -> TOptional<FSaveProfileReport> {
    if (profile_id != USpaceSaveSubsystem::slot_name()) {
        return {};
    }

    auto const* save{save_subsystem.get_save()};
    if (!IsValid(save)) {
        return {};
    }

    return save_profile::make_report(profile_id, save->score_records);
}
}
