#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestCapitalShipFightersTask.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"

struct TestCapitalShipFighterOrderQueue : public ml::FSoAArrayMixin {
    struct Order {
        uint8 task   : 1 {0};
        uint8 target : 1 {0};
    };

    TArray<FRegistryEntityHandle> handles;
    TArray<Order> orders;
    TArray<ETestCapitalShipFightersTask> tasks;
    TArray<FRegistryEntityHandle> targets;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.handles, self.orders, self.tasks, self.targets);
    }
};
