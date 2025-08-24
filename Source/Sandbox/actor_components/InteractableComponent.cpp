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
    for (auto& target : linked_interactables) {
        if (target) {
            target->interact(interactor);
        }
    }
}
auto UInteractableComponent::can_interact(AActor* const interactor) const -> EInteractReady {
    using enum EInteractReady;

    int32 const n_interactables{linked_interactables.Num()};
    if (!n_interactables) {
        return none_ready;
    }

    int32 ready_interactables{0};
    for (auto const& target : linked_interactables) {
        if (target->can_interact(interactor)) {
            ++ready_interactables;
        }
    }

    return ready_interactables == n_interactables ? all_ready : some_ready;
}
auto UInteractableComponent::try_interact(AActor* const interactor) -> EInteractTriggered {
    using enum EInteractTriggered;

    int32 const n_interactables{linked_interactables.Num()};
    if (!n_interactables) {
        return none_triggered;
    }

    int32 triggered_interactables{0};
    for (auto const& target : linked_interactables) {
        if (target->can_interact(interactor)) {
            ++triggered_interactables;
            target->interact(interactor);
        }
    }
    return triggered_interactables == n_interactables ? all_triggered : some_triggered;
}
