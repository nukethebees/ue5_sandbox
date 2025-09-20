#include "Sandbox/subsystems/RotationManagerSubsystem.h"
#include "Sandbox/actor_components/RotatingActorComponent.h"

void URotationManagerSubsystem::add(USceneComponent& scene_component,
                                    URotatingActorComponent& component) {
    UE_LOGFMT(LogTemp, Verbose, "Registering rotating actor.");

    dynamic_components.Add(&component);
    dynamic_scene_components.Add(&scene_component);

    tick_enabled = true;
}

void URotationManagerSubsystem::remove(URotatingActorComponent& rotating_component) {
    // Remove from dynamic arrays
    for (int32 i{dynamic_components.Num() - 1}; i >= 0; --i) {
        if (dynamic_components[i] == &rotating_component) {
            dynamic_components.RemoveAtSwap(i);
            dynamic_scene_components.RemoveAtSwap(i);
            break;
        }
    }

    // Check if we should disable ticking
    tick_enabled = dynamic_components.IsEmpty() && static_scene_components.IsEmpty();
}
void URotationManagerSubsystem::add(USceneComponent& scene_component, float speed) {
    static_scene_components.Add(&scene_component);
    static_speeds.Add(speed);
    tick_enabled = true;
}

void URotationManagerSubsystem::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    bool has_valid_actors{false};

    // Process dynamic rotations (read speed from component each tick)
    for (int32 i{dynamic_components.Num() - 1}; i >= 0; --i) {
        if (!dynamic_scene_components[i].IsValid() || !IsValid(dynamic_components[i])) {
            dynamic_components.RemoveAtSwap(i);
            dynamic_scene_components.RemoveAtSwap(i);
            continue;
        }

        auto const delta_rotation{FRotator(0.0f, dynamic_components[i]->speed * DeltaTime, 0.0f)};
        dynamic_scene_components[i]->AddLocalRotation(delta_rotation);
        has_valid_actors = true;
    }

    // Process static rotations (use cached speed values)
    for (int32 i{static_scene_components.Num() - 1}; i >= 0; --i) {
        if (!static_scene_components[i].IsValid()) {
            static_scene_components.RemoveAtSwap(i);
            static_speeds.RemoveAtSwap(i);
            continue;
        }

        auto const delta_rotation{FRotator(0.0f, static_speeds[i] * DeltaTime, 0.0f)};
        static_scene_components[i]->AddLocalRotation(delta_rotation);
        has_valid_actors = true;
    }

    if (!has_valid_actors) {
        tick_enabled = false;
    }
}

TStatId URotationManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(URotationManagerSubsystem, STATGROUP_Tickables);
}
