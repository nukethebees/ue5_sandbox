#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <HAL/Platform.h>

class UWorld;

class ATestEntityRegistry;
class ATestBatchOrchestrator;

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;

namespace ml {
struct TestSimulationDriver {
    explicit TestSimulationDriver(UWorld& world,
                                  ATestEntityRegistry& registry,
                                  ATestBatchOrchestrator& orchestrator)
        : world{world}
        , registry{registry}
        , orchestrator{orchestrator} {}

    static auto from_world(UWorld& world) -> TestSimulationDriver;

    auto get_capital_ships() const -> ATestCapitalShips const&;
    auto get_capital_ship_fighters() const -> ATestCapitalShipFighters const&;

    void queue_damage(FRegistryEntityHandle const target,
                      int32 const damage,
                      FRegistryEntityHandle const instigator);
    void queue_kill(FRegistryEntityHandle const target, FRegistryEntityHandle const instigator);

    void set_wait_until_tick_from_now(uint64 wait_cycles);

    UWorld& world;
    ATestEntityRegistry& registry;
    ATestBatchOrchestrator& orchestrator;

    uint64 tick_wait_end{0};
};
}