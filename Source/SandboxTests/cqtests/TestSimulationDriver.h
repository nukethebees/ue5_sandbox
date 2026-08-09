#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <SandboxCore/test_timeline.h>

#include <HAL/Platform.h>

class UWorld;
class AActor;

class ATestEntityRegistry;
class ATestBatchOrchestrator;

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;

namespace ml {
struct TestSimulationDriver {
    using time_type = double;

    explicit TestSimulationDriver(UWorld& world,
                                  ATestEntityRegistry& registry,
                                  ATestBatchOrchestrator& orchestrator);

    static auto from_world(UWorld& world) -> TestSimulationDriver;

    auto get_player_ship() const -> ATestSpaceShip const&;
    auto get_capital_ships() const -> ATestCapitalShips const&;
    auto get_capital_ship_fighters() const -> ATestCapitalShipFighters const&;

    void queue_damage(TConstArrayView<FRegistryEntityHandle> targets,
                      int32 damage,
                      FRegistryEntityHandle instigator = {});
    void queue_kills(TConstArrayView<FRegistryEntityHandle> targets,
                     FRegistryEntityHandle instigator = {});
    bool should_export_results() const;

    void set_time_scale(time_type scale);

    void set_wait_until_tick_from_now(uint64 wait_cycles);
    bool tick_wait_completed() const;

    auto get_time() const -> time_type;
    void set_delta_time_wait(time_type dt);
    void set_time_wait(time_type dt);
    bool time_wait_completed() const;

    UWorld& world;
    ATestEntityRegistry& registry;
    ATestBatchOrchestrator& orchestrator;

    uint64 tick_wait_end{0};
    time_type time_wait_end{0.f};
    time_type time_scale{100.f};
    FTestTimeline timeline;
};
}
