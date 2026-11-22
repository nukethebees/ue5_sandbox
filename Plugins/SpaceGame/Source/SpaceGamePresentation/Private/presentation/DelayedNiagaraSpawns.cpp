#include "SpaceGamePresentation/presentation/DelayedNiagaraSpawns.h"

#include <SandboxCore/array_utils.h>

#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void FDelayedNiagaraSpawns::AddReferencedObjects(FReferenceCollector& collector) {
    collector.AddReferencedObjects(systems);
}

void FDelayedNiagaraSpawns::update_spawns(float const dt, UWorld& world) {
    auto const pending_spawn_count{num()};

    if (pending_spawn_count <= 0) {
        return;
    }

    constexpr bool auto_activate{true};
    constexpr bool auto_destroy{true};

    for (auto index{pending_spawn_count - 1}; index >= 0; --index) {
        times_remaining[index] -= dt;

        if (times_remaining[index] > 0.0f) {
            continue;
        }

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(&world,
                                                       systems[index],
                                                       locations[index],
                                                       rotations[index],
                                                       scales[index],
                                                       auto_destroy,
                                                       auto_activate,
                                                       ENCPoolMethod::AutoRelease);
        remove_spawn_at(index);
    }
}

void FDelayedNiagaraSpawns::add_spawn(UNiagaraSystem* system,
                                      FVector const& location,
                                      FRotator const& rotation,
                                      FVector const& scale,
                                      float delay) {
    systems.Add(system);
    locations.Add(location);
    rotations.Add(rotation);
    scales.Add(scale);
    times_remaining.Add(delay);
}

auto FDelayedNiagaraSpawns::num() const -> int32 {
    return systems.Num();
}

void FDelayedNiagaraSpawns::remove_spawn_at(int32 index) {
    check(systems.IsValidIndex(index));

    ioj::remove_at_swap(
        index, 1, EAllowShrinking::No, systems, locations, rotations, scales, times_remaining);
}
