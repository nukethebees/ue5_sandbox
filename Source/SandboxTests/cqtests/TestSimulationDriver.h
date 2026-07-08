#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <HAL/Platform.h>

class UWorld;

class ATestEntityRegistry;

namespace ml {
struct TestSimulationDriver {

    explicit TestSimulationDriver(ATestEntityRegistry& registry, UWorld& world)
        : registry{registry}
        , world{world} {}

    void queue_damage(FRegistryEntityHandle const target,
                      int32 const damage,
                      FRegistryEntityHandle const instigator);

    ATestEntityRegistry& registry;
    UWorld& world;
};
}
