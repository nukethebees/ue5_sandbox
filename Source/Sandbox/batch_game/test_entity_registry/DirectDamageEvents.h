#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct DirectDamageEvents : public ml::FSoAArrayMixin {
    TArray<FRegistryEntityHandle> damaged_entities;
    TArray<int32> damage_amounts;
    TArray<FRegistryEntityHandle> instigators;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(
            self.damaged_entities, self.damage_amounts, self.instigators);
    }
};
