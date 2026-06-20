#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DelayedNiagaraSpawner.generated.h"

class UNiagaraSystem;

UCLASS()
class SANDBOX_API ADelayedNiagaraSpawner : public AActor {
    GENERATED_BODY()
  public:
    ADelayedNiagaraSpawner();

    void tick(float const dt);

    UFUNCTION()
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
  private:
    auto num() const -> int32;
    void remove_spawn_at(int32 index);

    UPROPERTY()
    TArray<UNiagaraSystem*> systems;

    UPROPERTY()
    TArray<FVector> locations;

    UPROPERTY()
    TArray<FRotator> rotations;

    UPROPERTY()
    TArray<FVector> scales;

    UPROPERTY()
    TArray<float> times_remaining;
};
