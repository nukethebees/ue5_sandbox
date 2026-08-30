#include "ExistingSaveGameBrowserSource.h"

#include "SpaceGame/persistence/SaveGameProfileAdapter.h"
#include "SpaceGame/persistence/SpaceSaveGame.h"
#include "SpaceGame/persistence/SpaceSaveSubsystem.h"

namespace ml::ioj::detail {
auto discover_existing_save_profiles(USpaceSaveSubsystem const& save_subsystem)
    -> TArray<FSaveProfileSummary> {
    auto const profiles{save_subsystem.get_profiles()};
    TArray<FSaveProfileSummary> summaries{};
    summaries.Reserve(profiles.Num());
    auto const& active_profile_id{save_subsystem.get_active_profile_id()};
    for (auto const& profile : profiles) {
        summaries.Add({
            .profile_id = profile.profile_id,
            .display_name = profile.display_name,
            .created_at = profile.created_at,
            .last_played_at = profile.last_played_at,
            .total_simulation_duration_seconds = profile.total_simulation_duration_seconds,
            .total_kills = profile.total_kills,
            .outcome_count = profile.outcome_count,
            .active = profile.profile_id == active_profile_id,
        });
    }
    return summaries;
}

auto load_existing_save_profile(USpaceSaveSubsystem const& save_subsystem,
                                FString const& profile_id) -> TOptional<FSaveProfileReport> {
    TArray<FScoreRecord> records{};
    if (!save_subsystem.load_profile_records(profile_id, records)) {
        return {};
    }
    return save_profile::make_report(profile_id, records);
}
}
