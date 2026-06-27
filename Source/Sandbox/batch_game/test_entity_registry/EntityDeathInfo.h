#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestDeathReason.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct EntityDeathInfo : public ml::FSoAArrayMixin {
    TArray<ETestDeathReason> reasons;
    TArray<FRegistryEntityHandle> victims;
    TArray<FRegistryEntityHandle> killers;

    void add(ETestDeathReason const reason,
             FRegistryEntityHandle const victim,
             FRegistryEntityHandle const killer);
    void add(ETestDeathReason const reason, FRegistryEntityHandle const victim) {
        add(reason, victim, FRegistryEntityHandle{});
    }

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.reasons, self.victims, self.killers);
    }
};
