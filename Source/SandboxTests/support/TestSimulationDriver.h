#pragma once
#include <ioj/sim/entity_unique_id.h>
#include <span>

#include <sandbox/core/test_timeline.h>

#include <HAL/Platform.h>

class UWorld;
class AActor;

class ATestBatchOrchestrator;

class ATestSpaceShip;

namespace ioj::sim {
class EntityLedger;
}

namespace ioj::sim::capital_ships {
struct Sim;
}

namespace ioj::sim::fighters {
struct Sim;
}

namespace ml {
struct TestSimulationDriver {
    using time_type = double;

    explicit TestSimulationDriver(UWorld& world, ATestBatchOrchestrator& orchestrator);

    static auto from_world(UWorld& world) -> TestSimulationDriver;

    auto get_world() const -> UWorld* { return &world; }
    auto get_player_ship() const -> ATestSpaceShip const&;
    auto get_capital_ships() const -> ::ioj::sim::capital_ships::Sim const&;
    auto get_fighters() const -> ::ioj::sim::fighters::Sim const&;

    void queue_damage(std::span<::ioj::sim::EntityUniqueId const> targets,
                      int32 damage,
                      ::ioj::sim::EntityUniqueId instigator = {});
    void queue_kills(std::span<::ioj::sim::EntityUniqueId const> targets,
                     ::ioj::sim::EntityUniqueId instigator = {});
    bool should_export_results() const;

    void set_time_scale(time_type scale);

    void set_wait_until_tick_from_now(uint64 wait_cycles);
    bool tick_wait_completed() const;

    auto get_time() const -> time_type;
    void advance_timeline() { timeline.tick(get_time()); }
    void set_delta_time_wait(time_type dt);
    void set_time_wait(time_type dt);
    bool time_wait_completed() const;

    UWorld& world;
    auto get_ledger() const -> ::ioj::sim::EntityLedger const&;
    ATestBatchOrchestrator& orchestrator;

    uint64 tick_wait_end{0};
    time_type time_wait_end{0.f};
    time_type time_scale{100.f};
    FTestTimeline timeline;
};
}
