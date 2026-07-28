#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestCapitalShipFightersTask.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"

struct TestCapitalShipFighterOrderQueue : public ml::FSoAArrayMixin {
    using Task = ETestCapitalShipFightersTask;

    struct Order {
        uint8 task   : 1 {0};
        uint8 target : 1 {0};
    };

    void add(FRegistryEntityHandle const handle,
             Order const order,
             Task const task,
             FRegistryEntityHandle const target) {
        handles.Add(handle);
        orders.Add(order);
        tasks.Add(task);
        targets.Add(target);
    }

    TArray<FRegistryEntityHandle> handles;
    TArray<Order> orders;
    TArray<ETestCapitalShipFightersTask> tasks;
    TArray<FRegistryEntityHandle> targets;

#define SANDBOX_PACK(STAMPER, NON_FINAL) \
    NON_FINAL(STAMPER(handles))          \
    NON_FINAL(STAMPER(orders))           \
    NON_FINAL(STAMPER(tasks))            \
    STAMPER(targets)

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};
