#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

void UDestructionManagerSubsystem::queue_destruction(AActor* actor) {
    queue_destruction(actor, queued_actors);
}

void UDestructionManagerSubsystem::queue_destruction(UActorComponent* component) {
    queue_destruction(component, queued_components);
}

void UDestructionManagerSubsystem::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    // Destroy queued actors
    for (auto& weak_actor : queued_actors) {
        if (weak_actor.IsValid()) {
            weak_actor->Destroy();
        }
    }
    queued_actors.Empty();

    // Destroy queued components
    for (auto& weak_component : queued_components) {
        if (weak_component.IsValid()) {
            weak_component->DestroyComponent();
        }
    }
    queued_components.Empty();

    // Disable ticking since queues are now empty
    tick_enabled = false;
}

TStatId UDestructionManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDestructionManagerSubsystem, STATGROUP_Tickables);
}
