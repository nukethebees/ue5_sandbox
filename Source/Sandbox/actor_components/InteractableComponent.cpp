#include "Sandbox/actor_components/InteractableComponent.h"

UInteractableComponent::UInteractableComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* const owner{GetOwner()};
    if (owner == nullptr) {
        return;
    }

    // Search all components on the same actor
    auto const& components{owner->GetComponents()};
    for (auto* const component : components) {
        if (component == nullptr) {
            continue;
        }

        // Check if the component implements the interface
        if (component->GetClass()->ImplementsInterface(UInteractable::StaticClass())) {
            linked_interactables.Add(TScriptInterface<IInteractable>(component));
        }
    }
}

void UInteractableComponent::interact(AActor* const interactor) {
    auto const* const owner{GetOwner()};
    if (!owner) {
        return;
    }

    for (auto& target : linked_interactables) {
        if (target) {
            target->interact(interactor);
        }
    }
}
bool UInteractableComponent::can_interact(AActor* const interactor) {
    for (auto& target : linked_interactables) {
        if (target->can_interact(interactor)) {
            return true;
        }
    }
    return false;
}
bool UInteractableComponent::try_interact(AActor* const interactor) {
    bool any_true{false};

    for (auto& target : linked_interactables) {
        if (target->can_interact(interactor)) {
            any_true = true;
            target->interact(interactor);
        }
    }
    return any_true;
}
