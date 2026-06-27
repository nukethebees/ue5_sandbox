#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

class AActor;
class UActorComponent;

struct UnresolvedDamageEvents : public ml::FSoAArrayMixin {
    TArray<AActor*> damaged_actors;
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.damaged_actors,
                                         self.damage_amounts,
                                         self.actor_components,
                                         self.hit_items,
                                         self.instigators);
    }
};

struct DamageEvents : public ml::FSoAArrayMixin {
    TArray<int32> damage_amounts;
    TArray<UActorComponent*> actor_components;
    TArray<int32> hit_items;
    TArray<FRegistryEntityHandle> instigators;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(
            self.damage_amounts, self.actor_components, self.hit_items, self.instigators);
    }
};
