#include "Sandbox/actor_components/HealthStationComponent.h"

#include "Sandbox/subsystems/TriggerSubsystem.h"

UHealthStationComponent::UHealthStationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthStationComponent::BeginPlay() {
    Super::BeginPlay();

    // Initialize payload capacity
    health_station_payload.reset_capacity();

    // Bind delegate for UI updates (two-phase initialization)
    health_station_payload.on_state_changed.BindUObject(this, &UHealthStationComponent::broadcast_state);

    // Register with trigger subsystem
    auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()};
    if (subsystem) {
        my_trigger_id = subsystem->register_triggerable(GetOwner(), health_station_payload);
    }
}
void UHealthStationComponent::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(GetOwner());
    }
    Super::EndPlay(reason);
}
void UHealthStationComponent::broadcast_state() const {
    FStationStateData state{.remaining_capacity = health_station_payload.current_capacity,
                            .cooldown_remaining = health_station_payload.cooldown_remaining,
                            .cooldown_total = health_station_payload.cooldown_duration,
                            .is_ready = health_station_payload.is_ready()};

    on_station_state_changed.Broadcast(state);
}
