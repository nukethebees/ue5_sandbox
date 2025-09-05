#include "Sandbox/actor_components/HealthStationComponent.h"

UHealthStationComponent::UHealthStationComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthStationComponent::BeginPlay() {
    Super::BeginPlay();
}
