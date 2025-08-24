#include "Sandbox/actor_components/InteractToActivateComponent.h"

UInteractToActivateComponent::UInteractToActivateComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractToActivateComponent::BeginPlay() {
    Super::BeginPlay();

    if (!activator) {
        activator = GetOwner()->FindComponentByClass<UActivatorComponent>();
    }
}

void UInteractToActivateComponent::interact(AActor* interactor) {
    if (activator) {
        activator->trigger_activation(interactor);
    }
}
