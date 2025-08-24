#include "Sandbox/actor_components/ActivatorComponent.h"

UActivatorComponent::UActivatorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActivatorComponent::BeginPlay() {
    Super::BeginPlay();
}
