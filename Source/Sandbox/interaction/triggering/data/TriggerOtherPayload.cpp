#include "Sandbox/data/trigger/TriggerOtherPayload.h"

#include "Sandbox/subsystems/world/TriggerSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

bool FTriggerOtherPayload::add_target_actor(ActorId actor_id) {
    RETURN_VALUE_IF_FALSE(n_targets < MAX_TARGETS, false);

    target_actor_ids[n_targets++] = actor_id;
    return true;
}

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
        .type = ETriggerSourceType::Activation,
        .capabilities = {},
        .instigator = context.source.get_instigator(),
        .source_location = context.triggered_actor.GetActorLocation(),
        .source_triggerable = {} // Will be set to this actor's ID if available
    };

    // Add mechanical capability for button/switch triggers
    new_source.capabilities.add_capability(ETriggerSourceCapability::Mechanical);

    // Try to get this actor's first ID for proper chain tracking
    auto triggerable_ids{subsystem->get_triggerable_ids(context.triggered_actor)};
    if (!triggerable_ids.empty()) {
        new_source.source_triggerable = triggerable_ids[0];
    }

    for (uint8 i{0}; i < n_targets; ++i) {
        LOG.log_verbose(TEXT("Triggering target %d."), i);

        ActorId actor_id{target_actor_ids[i]};
        if (activation_delay > 0.0f) {
            LOG.log_verbose(TEXT("Delayed triggering not yet implemented, triggering immediately"));
            subsystem->trigger(actor_id, new_source);
        } else {
            subsystem->trigger(actor_id, new_source);
        }
    }

    return {false}; // No ticking needed for trigger other
}
