#include "Sandbox/health/HealthStationPayload.h"

#include "Sandbox/health/DamageManagerSubsystem.h"
#include "Sandbox/health/HealthComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

FTriggerResult FHealthStationPayload::trigger(FTriggerContext context) {
    if (!can_trigger(context)) {
        logger.log_verbose(TEXT("Health station cannot be triggered"));
        return {false};
    }

    auto* const health{context.source.get_instigator()->FindComponentByClass<UHealthComponent>()};
    if (!health || health->at_max_health()) {
        logger.log_verbose(TEXT("Interactor has no health component or is at max health"));
        return {false};
    }

    auto* const manager{context.world.GetSubsystem<UDamageManagerSubsystem>()};
    if (!manager) {
        logger.log_warning(TEXT("No UDamageManagerSubsystem found"));
        return {false};
    }

    FHealthChange change{FMath::Min(heal_amount_per_use, current_capacity),
                         EHealthChangeType::Healing};
    manager->queue_health_change(health, change);

    current_capacity = FMath::Max(0.0f, current_capacity - change.value);
    cooldown_remaining = cooldown_duration;

    broadcast_state();

    logger.log_verbose(TEXT("Health station triggered: healed %.1f, remaining capacity %.1f"),
                       change.value,
                       current_capacity);

    return {true}; // Start cooldown ticking
}

bool FHealthStationPayload::can_trigger(FTriggerContext const& context) const {
    if (current_capacity <= 0.0f) {
        return false;
    }

    if (cooldown_remaining > 0.0f) {
        return false;
    }

    auto* instigator{context.source.get_instigator()};
    if (!instigator) {
        return false;
    }

    auto* const health{instigator->FindComponentByClass<UHealthComponent>()};
    return health && !health->at_max_health();
}

bool FHealthStationPayload::tick(float delta_time) {
    if (cooldown_remaining > 0.0f) {
        cooldown_remaining = FMath::Max(0.0f, cooldown_remaining - delta_time);
        broadcast_state();
        return cooldown_remaining > 0.0f;
    }

    return false;
}

void FHealthStationPayload::broadcast_state() const {
    FStationStateData state{.remaining_capacity = current_capacity,
                            .cooldown_remaining = cooldown_remaining,
                            .cooldown_total = cooldown_duration,
                            .is_ready = is_ready()};

    on_station_state_changed.Broadcast(state);
}
