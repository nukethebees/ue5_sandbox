#include "Sandbox/actor_components/ActivatorComponent.h"

#include "GameFramework/Actor.h"

UActivatorComponent::UActivatorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActivatorComponent::BeginPlay() {
    Super::BeginPlay();

    for (auto* const actor : linked_actors) {
        if (!actor) {
            continue;
        }

        for (auto* const component : actor->GetComponents()) {
            if (component &&
                component->GetClass()->ImplementsInterface(UActivatable::StaticClass())) {
                linked_activatables.Add(TScriptInterface<IActivatable>(component));
            }
        }
    }
}

void UActivatorComponent::trigger_activation(AActor* instigator) {
    for (auto& target : linked_activatables) {
        if (target) {
            target->trigger_activation(instigator);
        }
    }
}
