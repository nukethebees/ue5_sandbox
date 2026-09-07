#include "SpaceGame/input/ControlProfiles.h"

#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace ml::ioj {
namespace control_profile_details {
inline TArray<FControlProfileDefinition> const profiles{
    {TEXT("InputUserSettings.Profiles.Default"),
     NSLOCTEXT("Controls", "DefaultProfile", "Default")},
    {TEXT("SpaceGame.Controls.TwinStickAimMove"),
     NSLOCTEXT("Controls", "TwinStickAimMoveProfile", "Twin-stick: Aim / Move")},
    {TEXT("SpaceGame.Controls.TwinStickMoveAim"),
     NSLOCTEXT("Controls", "TwinStickMoveAimProfile", "Twin-stick: Move / Aim (Southpaw)")},
    {TEXT("SpaceGame.Controls.TwinStickZRollAim"),
     NSLOCTEXT("Controls", "TwinStickZRollAimProfile", "Twin-stick: Z/Roll / Aim")},
};
} // namespace control_profile_details

auto control_profile_definitions() -> TConstArrayView<FControlProfileDefinition> {
    return control_profile_details::profiles;
}

auto register_control_profiles(UEnhancedInputUserSettings& settings,
                               UInputMappingContext& mapping_context) -> bool {
    if (!settings.IsMappingContextRegistered(&mapping_context)) {
        settings.RegisterInputMappingContext(&mapping_context);
    }

    auto success{true};
    for (auto const& definition : control_profile_details::profiles) {
        auto* const profile{settings.GetKeyProfileWithId(definition.id)};
        if (!IsValid(profile)) {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("Control profile '%s' was not created by the mapping context"),
                   *definition.id);
            success = false;
            continue;
        }
        profile->SetDisplayName(definition.display_name);
    }
    return success;
}

auto cycle_control_profile(UEnhancedInputUserSettings& settings) -> bool {
    auto const& profiles{control_profile_details::profiles};
    auto const active_index{profiles.IndexOfByPredicate([&settings](auto const& definition) {
        return definition.id == settings.GetActiveKeyProfileId();
    })};
    auto const next_index{active_index == INDEX_NONE ? 0 : (active_index + 1) % profiles.Num()};
    return settings.SetActiveKeyProfile(profiles[next_index].id);
}

} // namespace ml::ioj
