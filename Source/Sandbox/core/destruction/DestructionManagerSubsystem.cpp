#include "Sandbox/core/destruction/DestructionManagerSubsystem.h"

#include "Engine/World.h"

void UDestructionManagerSubsystem::queue_destruction(AActor* actor) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDestructionManagerSubsystem::queue_destruction"))
    queue_destruction(actor, queued_actors);
}

void UDestructionManagerSubsystem::queue_destruction(UActorComponent* component) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDestructionManagerSubsystem::queue_destruction"))
    queue_destruction(component, queued_components);
}

void UDestructionManagerSubsystem::queue_destruction_with_delay(AActor* actor, float delay) {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::UDestructionManagerSubsystem::queue_destruction_with_delay"))
    log_verbose(TEXT("Logging with delay."));

    if (!actor || delay <= 0.0f) {
        queue_destruction(actor);
        return;
    }

    auto const destruction_id{next_destruction_id++};
    FTimerHandle timer_handle{};
    GetWorld()->GetTimerManager().SetTimer(
        timer_handle,
        [this, destruction_id]() { destroy_actor_delayed(destruction_id); },
        delay,
        false);

    delayed_actors.Add(destruction_id, actor);
    active_timers.Add(destruction_id, timer_handle);
}

void UDestructionManagerSubsystem::queue_destruction_with_delay(UActorComponent* component,
                                                                float delay) {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::UDestructionManagerSubsystem::queue_destruction_with_delay"))
    if (!component || delay <= 0.0f) {
        queue_destruction(component);
        return;
    }

    auto const destruction_id{next_destruction_id++};
    FTimerHandle timer_handle{};
    GetWorld()->GetTimerManager().SetTimer(
        timer_handle,
        [this, destruction_id]() { destroy_component_delayed(destruction_id); },
        delay,
        false);

    delayed_components.Add(destruction_id, component);
    active_timers.Add(destruction_id, timer_handle);
}

void UDestructionManagerSubsystem::Tick(float DeltaTime) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDestructionManagerSubsystem::Tick"))
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

void UDestructionManagerSubsystem::destroy_actor_delayed(int32 destruction_id) {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::UDestructionManagerSubsystem::destroy_actor_delayed"))
    log_verbose(TEXT("destroy_actor_delayed with ID %d."), destruction_id);

    if (auto* actor_ptr{delayed_actors.Find(destruction_id)}) {
        if (actor_ptr->IsValid()) {
            log_verbose(TEXT("Destroying the actor."));
            (*actor_ptr)->Destroy();
        } else {
            log_warning(TEXT("Actor not valid."));
        }

        log_verbose(TEXT("Removing the actor from the map."));
        delayed_actors.Remove(destruction_id);
        active_timers.Remove(destruction_id);
    } else {
        log_warning(TEXT("Couldn't find the actor with ID %d."), destruction_id);
    }
}

void UDestructionManagerSubsystem::destroy_component_delayed(int32 destruction_id) {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::UDestructionManagerSubsystem::destroy_component_delayed"))
    if (auto* component_ptr{delayed_components.Find(destruction_id)}) {
        if (component_ptr->IsValid()) {
            (*component_ptr)->DestroyComponent();
        }
        delayed_components.Remove(destruction_id);
        active_timers.Remove(destruction_id);
    }
}

TStatId UDestructionManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDestructionManagerSubsystem, STATGROUP_Tickables);
}
