#include "Sandbox/data/TriggerOtherPayload.h"

#include "Sandbox/subsystems/TriggerSubsystem.h"

FTriggerResult FTriggerOtherPayload::trigger(FTriggerContext context) {
    log_verbose(TEXT("Triggering %d targets"), n_targets);

    auto* subsystem{context.world.GetSubsystem<UTriggerSubsystem>()};
    if (!subsystem) {
        log_warning(TEXT("No UTriggerSubsystem found"));
        return {.enable_ticking = false};
    }

    // Create new source for targets, preserving original instigator
    FTriggeringSource new_source{
        .type = ETriggerForm::Activation,
        .capabilities = {}, // No specific capabilities for simple activation
        .instigator = context.source.get_instigator(),
        .source_location = context.triggered_actor.GetActorLocation(),
        .source_triggerable = {} // Will be set to this actor's ID if available
    };

    // Try to get this actor's ID for proper chain tracking
    if (auto this_id{subsystem->get_triggerable_id(&context.triggered_actor)}) {
        new_source.source_triggerable = *this_id;
    }

    for (uint8 i{0}; i < n_targets; ++i) {
        if (targets[i].is_valid()) {
            if (activation_delay > 0.0f) {
                // TODO: Implement delayed triggering when needed
                log_verbose(TEXT("Delayed triggering not yet implemented, triggering immediately"));
                subsystem->trigger(targets[i], new_source);
            } else {
                subsystem->trigger(targets[i], new_source);
            }
        }
    }

    return {.enable_ticking = false}; // No ticking needed for trigger other
}
