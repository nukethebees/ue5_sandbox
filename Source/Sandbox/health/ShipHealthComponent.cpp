#include "Sandbox/health/ShipHealthComponent.h"

UShipHealthComponent::UShipHealthComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UShipHealthComponent::BeginPlay() {
    Super::BeginPlay();

    health.health = health.max_health;
}
void UShipHealthComponent::set_health(int32 new_health) {
    health.health = new_health;
    on_health_changed.Execute(health);
}
