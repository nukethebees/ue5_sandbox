#include "ForcefieldPayload.h"

#include "Sandbox/environment/obstacles/ForcefieldActor.h"

FTriggerResult FForcefieldPayload::trigger(FTriggerContext context) {
    logger.log_verbose(TEXT("Triggering forcefield for actor %s"),
                       *context.triggered_actor.GetName());

    // Access control: only allow mechanical triggers (buttons, switches, etc.)
    if (!context.source.has_capability(ETriggerSourceCapability::Mechanical)) {
        logger.log_verbose(
            TEXT("Rejecting trigger - forcefield only accepts Mechanical capability"));
        return FTriggerResult(false);
    }

    if (auto* const actor{Cast<AForcefieldActor>(&context.triggered_actor)}) {
        actor->trigger_activation();
    }

    return FTriggerResult(false);
}

bool FForcefieldPayload::tick(float delta_time) {
    // Since we're delegating everything to the actor, no ticking needed in the payload
    return false;
}
