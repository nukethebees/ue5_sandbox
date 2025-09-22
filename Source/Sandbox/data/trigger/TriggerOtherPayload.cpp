#include "Sandbox/data/trigger/TriggerOtherPayload.h"

#include "Sandbox/subsystems/TriggerSubsystem.h"

FTriggerResult FTriggerOtherPayload::trigger(FTriggerContext context) {
    static constexpr auto LOG{NestedLogger<"trigger">()};

    LOG.log_verbose(TEXT("Triggering %d targets"), n_targets);

    auto* subsystem{context.world.GetSubsystem<UTriggerSubsystem>()};
    if (!subsystem) {
        LOG.log_warning(TEXT("No UTriggerSubsystem found"));
        return {false};
    }

    // Create new source for targets, preserving original instigator
    FTriggeringSource new_source{
        .type = ETriggerForm::Activation,
        .capabilities = {}, // No specific capabilities for simple activation
        .instigator = context.source.get_instigator(),
        .source_location = context.triggered_actor.GetActorLocation(),
        .source_triggerable = {} // Will be set to this actor's ID if available
    };

    // Try to get this actor's first ID for proper chain tracking
    auto triggerable_ids{subsystem->get_triggerable_ids(context.triggered_actor)};
    if (!triggerable_ids.empty()) {
        new_source.source_triggerable = triggerable_ids[0];
    }

    for (uint8 i{0}; i < n_targets; ++i) {
        LOG.log_verbose(TEXT("Triggering target %d."), i);

        ActorId actor_id{target_actor_ids[i]};
        if (actor_id != 0) {
            if (activation_delay > 0.0f) {
                // TODO: Implement delayed triggering when needed
                LOG.log_verbose(
                    TEXT("Delayed triggering not yet implemented, triggering immediately"));
                subsystem->trigger_actor(actor_id, new_source);
            } else {
                subsystem->trigger_actor(actor_id, new_source);
            }
        } else {
            LOG.log_verbose(TEXT("Actor id is zero."));
        }
    }

    return {false}; // No ticking needed for trigger other
}
