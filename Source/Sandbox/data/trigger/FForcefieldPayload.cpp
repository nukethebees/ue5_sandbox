#include "FForcefieldPayload.h"

#include "Sandbox/actors/ForcefieldActor.h"

FTriggerResult FForcefieldPayload::trigger(FTriggerContext context) {
    logger.log_verbose(TEXT("Triggering forcefield for actor %s"),
                       *context.triggered_actor.GetName());

    if (auto* const actor{Cast<AForcefieldActor>(&context.triggered_actor)}) {
        actor->trigger_activation();
    }

    return FTriggerResult(false);
}

bool FForcefieldPayload::tick(float delta_time) {
    // Since we're delegating everything to the actor, no ticking needed in the payload
    return false;
}
