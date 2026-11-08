#include "SandboxCelestials/CelestialBackdropProfile.h"

#if WITH_EDITOR
#include "UObject/UObjectIterator.h"
#endif

void ioj::FCelestialBackdropAppearanceSettings::apply_to(
    ioj::FCelestialBackdropSettings& target) const {
    target.terminator_softness = terminator_softness;
    target.surface = surface;
    target.accents = accents;
    target.clouds = clouds;
    target.atmosphere = atmosphere;
    target.rings = rings;
}

ioj::FCelestialBackdropAppearanceSettings ioj::FCelestialBackdropAppearanceSettings::from_settings(
    ioj::FCelestialBackdropSettings const& source) {
    auto result{ioj::FCelestialBackdropAppearanceSettings{}};
    result.terminator_softness = source.terminator_softness;
    result.surface = source.surface;
    result.accents = source.accents;
    result.clouds = source.clouds;
    result.atmosphere = source.atmosphere;
    result.rings = source.rings;
    return result;
}

namespace ioj {
void UCelestialBackdropProfile::apply_to(FCelestialBackdropSettings& target) const {
    appearance.apply_to(target);
}

#if WITH_EDITOR
void UCelestialBackdropProfile::PostEditChangeProperty(
    FPropertyChangedEvent& property_changed_event) {
    Super::PostEditChangeProperty(property_changed_event);

    for (TObjectIterator<ACelestialBackdropActor> actor_iterator{}; actor_iterator;
         ++actor_iterator) {
        auto* const actor{*actor_iterator};
        if (!actor->IsTemplate() && actor->profile == this && !actor->override_profile_appearance) {
            actor->apply_settings();
        }
    }
}
#endif

} // namespace ioj
