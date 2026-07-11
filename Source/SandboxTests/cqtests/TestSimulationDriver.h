#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <HAL/Platform.h>

class UWorld;

class ATestEntityRegistry;
class ATestBatchOrchestrator;

namespace ml {
struct TestSimulationDriver {
    explicit TestSimulationDriver(UWorld& world,
                                  ATestEntityRegistry& registry,
                                  ATestBatchOrchestrator& orchestrator)
        : world{world}
        , registry{registry}
        , orchestrator{orchestrator} {}

    static auto from_world(UWorld& world) -> TestSimulationDriver;

    void queue_damage(FRegistryEntityHandle const target,
                      int32 const damage,
                      FRegistryEntityHandle const instigator);

    UWorld& world;
    ATestEntityRegistry& registry;
    ATestBatchOrchestrator& orchestrator;
};
}
