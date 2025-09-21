#include "Sandbox/actor_components/HealthStationComponent.h"

#include "Sandbox/actors/HealthStationActor.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"

UHealthStationComponent::UHealthStationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthStationComponent::BeginPlay() {
    Super::BeginPlay();

    health_station_payload.reset_capacity();

    auto* owner{GetOwner()};

    health_station_payload.on_station_state_changed.AddDynamic(
        Cast<AHealthStationActor>(owner), &AHealthStationActor::handle_station_state_changed);
    health_station_payload.broadcast_state();

    auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()};
    if (subsystem) {
        my_trigger_id =
            subsystem->register_triggerable(GetOwner(), std::move(health_station_payload));
    }
}
void UHealthStationComponent::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(GetOwner());
    }
    Super::EndPlay(reason);
}
