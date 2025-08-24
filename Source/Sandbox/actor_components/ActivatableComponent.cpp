#include "Sandbox/actor_components/ActivatableComponent.h"

UActivatableComponent::UActivatableComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UActivatableComponent::BeginPlay() {
    Super::BeginPlay();

    auto const* owner{GetOwner()};
    if (!owner)
        return;

    for (auto* component : owner->GetComponents()) {
        if (component && component->GetClass()->ImplementsInterface(UActivatable::StaticClass())) {
            linked_activatables.Add(TScriptInterface<IActivatable>(component));
        }
    }
}

void UActivatableComponent::trigger_activation(AActor* instigator) {
    for (auto const& target : linked_activatables) {
        if (target) {
            target->trigger_activation(instigator);
        }
    }
}

auto UActivatableComponent::can_activate(AActor const* instigator) const -> EActivateReady {
    using enum EActivateReady;

    int32 const total{linked_activatables.Num()};
    if (total == 0) {
        return none_ready;
    }

    int32 ready = 0;
    for (auto const& target : linked_activatables) {
        if (target->can_activate(instigator)) {
            ++ready;
        }
    }

    return ready == total ? all_ready : some_ready;
}

auto UActivatableComponent::try_activate(AActor* instigator) -> EActivateTriggered {
    using enum EActivateTriggered;

    int32 const total{linked_activatables.Num()};
    if (total == 0) {
        return none_triggered;
    }

    int32 triggered = 0;
    for (auto const& target : linked_activatables) {
        if (target->can_activate(instigator)) {
            ++triggered;
            target->trigger_activation(instigator);
        }
    }

    return triggered == total ? all_triggered : some_triggered;
}
