#include "Sandbox/subsystems/RotationManagerSubsystem.h"
#include "Sandbox/actor_components/RotatingActorComponent.h"

void URotationManagerSubsystem::register_rotating_actor(URotatingActorComponent* component,
                                                        AActor* actor,
                                                        ERotationType rotation_type) {
    if (!component || !actor) {
        return;
    }

    if (rotation_type == ERotationType::DYNAMIC) {
        dynamic_components.Add(component);
        dynamic_actors.Add(actor);
    } else {
        static_actors.Add(actor);
        static_speeds.Add(component->speed);
    }

    tick_enabled = true;
}

void URotationManagerSubsystem::unregister_rotating_actor(URotatingActorComponent* component) {
    if (!component) {
        return;
    }

    // Remove from dynamic arrays
    for (int32 i{dynamic_components.Num() - 1}; i >= 0; --i) {
        if (dynamic_components[i] == component) {
            dynamic_components.RemoveAtSwap(i);
            dynamic_actors.RemoveAtSwap(i);
            break;
        }
    }

    // Check if we should disable ticking
    tick_enabled = dynamic_components.IsEmpty() && static_actors.IsEmpty();
}

void URotationManagerSubsystem::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    bool has_valid_actors{false};

    // Process dynamic rotations (read speed from component each tick)
    for (int32 i{dynamic_components.Num() - 1}; i >= 0; --i) {
        if (!dynamic_actors[i].IsValid() || !IsValid(dynamic_components[i])) {
            dynamic_components.RemoveAtSwap(i);
            dynamic_actors.RemoveAtSwap(i);
            continue;
        }

        auto const delta_rotation{FRotator(0.0f, dynamic_components[i]->speed * DeltaTime, 0.0f)};
        dynamic_actors[i]->AddActorLocalRotation(delta_rotation);
        has_valid_actors = true;
    }

    // Process static rotations (use cached speed values)
    for (int32 i{static_actors.Num() - 1}; i >= 0; --i) {
        if (!static_actors[i].IsValid()) {
            static_actors.RemoveAtSwap(i);
            static_speeds.RemoveAtSwap(i);
            continue;
        }

        auto const delta_rotation{FRotator(0.0f, static_speeds[i] * DeltaTime, 0.0f)};
        static_actors[i]->AddActorLocalRotation(delta_rotation);
        has_valid_actors = true;
    }

    if (!has_valid_actors) {
        tick_enabled = false;
    }
}

TStatId URotationManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(URotationManagerSubsystem, STATGROUP_Tickables);
}
