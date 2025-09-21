#include "FForcefieldPayload.h"

#include "Sandbox/actors/ForcefieldActor.h"

FTriggerResult FForcefieldPayload::trigger(FTriggerContext context) {
    logger.log_verbose(TEXT("Triggering forcefield for actor %s"),
                       *context.triggered_actor.GetName());

    forcefield_actor = &context.triggered_actor;

    // Delegate to the existing AForcefieldActor trigger_activation method
    if (auto* const actor{Cast<AForcefieldActor>(forcefield_actor.Get())}) {
        actor->trigger_activation();
    }

    FTriggerResult result{};
    result.enable_ticking = false; // Let the actor handle its own timing
    return result;
}

bool FForcefieldPayload::tick(float delta_time) {
    // Since we're delegating everything to the actor, no ticking needed in the payload
    return false;
}
