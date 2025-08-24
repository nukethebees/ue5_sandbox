#include "Sandbox/actor_components/ActivatableComponent.h"

UActivatableComponent::UActivatableComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActivatableComponent::BeginPlay() {
    Super::BeginPlay();
}
