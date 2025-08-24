#include "Sandbox/actor_components/InteractableComponent.h"

UInteractableComponent::UInteractableComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::BeginPlay() {
    Super::BeginPlay();
}

void UInteractableComponent::interact(AActor* const interactor) {
    auto const* const owner{GetOwner()};
    if (!owner) {
        return;
    }
}
