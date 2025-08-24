#include "Sandbox/actor_components/InteractorComponent.h"

UInteractorComponent::UInteractorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractorComponent::BeginPlay() {
    Super::BeginPlay();
}
