#include "Sandbox/environment/effects/DelayedNiagaraSpawner.h"

#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ADelayedNiagaraSpawner::ADelayedNiagaraSpawner() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ADelayedNiagaraSpawner::tick(float const dt) {
    auto* world{GetWorld()};
    auto const pending_spawn_count{num()};

    if (!IsValid(world) || (pending_spawn_count <= 0)) { return; }

    constexpr bool auto_activate{true};
    constexpr bool auto_destroy{true};

    for (auto index{pending_spawn_count - 1}; index >= 0; --index) {
        times_remaining[index] -= dt;

        if (times_remaining[index] > 0.0f) { continue; }

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
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

void ADelayedNiagaraSpawner::add_spawn(UNiagaraSystem* system,
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
void ADelayedNiagaraSpawner::add_spawns(TArrayView<UNiagaraSystem*> new_systems,
                                        TConstArrayView<FVector> new_locations,
                                        TConstArrayView<FRotator> new_rotations,
                                        TConstArrayView<FVector> new_scales,
                                        TConstArrayView<float> new_delays) {
    systems.Append(new_systems);
    locations.Append(new_locations);
    rotations.Append(new_rotations);
    scales.Append(new_scales);
    times_remaining.Append(new_delays);
}

auto ADelayedNiagaraSpawner::num() const -> int32 {
    return systems.Num();
}

void ADelayedNiagaraSpawner::remove_spawn_at(int32 index) {
    check(systems.IsValidIndex(index));

    systems.RemoveAtSwap(index, EAllowShrinking::No);
    locations.RemoveAtSwap(index, EAllowShrinking::No);
    rotations.RemoveAtSwap(index, EAllowShrinking::No);
    scales.RemoveAtSwap(index, EAllowShrinking::No);
    times_remaining.RemoveAtSwap(index, EAllowShrinking::No);
}
