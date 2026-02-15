#include "Sandbox/health/ShipHealthComponent.h"

UShipHealthComponent::UShipHealthComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UShipHealthComponent::BeginPlay() {
    Super::BeginPlay();

    health.health = health.max_health;
}
void UShipHealthComponent::set_health(int32 new_health) {
    health.health = FMath::Clamp(new_health, 0, health.max_health);
    on_health_changed.Execute(health);
}
void UShipHealthComponent::add_health(int32 new_health) {
    set_health(health.health + new_health);
}
void UShipHealthComponent::upgrade_max_health() {
    health.upgrade_max_health();
}
