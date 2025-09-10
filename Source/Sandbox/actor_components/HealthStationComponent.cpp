#include "Sandbox/actor_components/HealthStationComponent.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"

UHealthStationComponent::UHealthStationComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}
void UHealthStationComponent::BeginPlay() {
    Super::BeginPlay();
    reset_current_capacity();
}
void UHealthStationComponent::TickComponent(float DeltaTime,
                                            ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    cooldown_remaining = FMath::Max(0.0f, cooldown_remaining - DeltaTime);

    if (cooldown_remaining <= 0.0f) {
        cooldown_remaining = 0.0f;
        SetComponentTickEnabled(false);
    }

    broadcast_state();
}
void UHealthStationComponent::interact(AActor* interactor) {
    if (!interactor || !can_interact(interactor)) {
        return;
    }

    auto* const health{interactor->FindComponentByClass<UHealthComponent>()};
    if (!health || health->at_max_health()) {
        return;
    }

    if (auto* const manager{GetWorld()->GetSubsystem<UDamageManagerSubsystem>()}) {
        FHealthChange change{heal_amount_per_use, EHealthChangeType::Healing};
        manager->queue_damage(health, change);

        current_capacity = FMath::Max(0.0f, current_capacity - change.value);
        start_cooldown();
        broadcast_state();
    }
}
bool UHealthStationComponent::can_interact(AActor const* interactor) const {
    return cooldown_remaining <= 0.0f;
}
void UHealthStationComponent::reset_current_capacity() {
    current_capacity = max_capacity;
}
void UHealthStationComponent::broadcast_state() const {
    on_station_state_changed.Broadcast(
        {current_capacity, cooldown_remaining, cooldown_duration, can_interact(nullptr)});
}
void UHealthStationComponent::start_cooldown() {
    cooldown_remaining = cooldown_duration;
    SetComponentTickEnabled(true);
}
