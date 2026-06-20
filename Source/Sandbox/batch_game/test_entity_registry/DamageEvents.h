#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

class AActor;
class UActorComponent;

struct UnresolvedDamageEvents {
    TArray<AActor*> damaged_actors;
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    auto num() const -> int32;
    void reset();
    void validate_array_sizes() const;
};

struct DamageEvents {
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    auto num() const -> int32;
    void reset();
    void
        remove_at_swap(int32 const index, int32 const count, EAllowShrinking const allow_shrinking);

    void validate_array_sizes() const;
};
