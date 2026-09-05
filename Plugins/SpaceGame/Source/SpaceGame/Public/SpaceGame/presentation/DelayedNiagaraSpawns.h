#pragma once

#include <UObject/GCObject.h>
#include "CoreMinimal.h"

class UWorld;

class UNiagaraSystem;

struct SPACEGAME_API FDelayedNiagaraSpawns : public FGCObject {
  public:
    void AddReferencedObjects(FReferenceCollector& collector) override;
    auto GetReferencerName() const -> FString override { return TEXT("FDelayedNiagaraSpawns"); }

    void update_spawns(float dt, UWorld& world);

    void add_spawn(UNiagaraSystem* system,
                   FVector const& location,
                   FRotator const& rotation,
                   FVector const& scale,
                   float delay);
    void add_spawns(TArrayView<UNiagaraSystem*> new_systems,
                    TConstArrayView<FVector> new_locations,
                    TConstArrayView<FRotator> new_rotations,
                    TConstArrayView<FVector> new_scales,
                    TConstArrayView<float> new_delays);
    void add_spawns(TArrayView<UNiagaraSystem*> new_systems,
                    TConstArrayView<FVector> new_locations,
                    TConstArrayView<float> new_delays);
  private:
    auto num() const -> int32;
    void remove_spawn_at(int32 index);

    TArray<TObjectPtr<UNiagaraSystem>> systems;

    TArray<FVector> locations;

    TArray<FRotator> rotations;

    TArray<FVector> scales;

    TArray<float> times_remaining;
};
