#include "TestSimulationDriver.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/fighters/sim.h>
#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SandboxTests/SandboxTestLogCategories.h>
#include <SandboxTests/support/TestBatchOrchestratorTestAccess.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/PrimitiveComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <EngineUtils.h>

#include <limits>

namespace ioj {
TestSimulationDriver::TestSimulationDriver(UWorld& world, ATestBatchOrchestrator& orchestrator)
    : world{world}
    , orchestrator{orchestrator} {}

auto TestSimulationDriver::from_world(UWorld& world) -> TestSimulationDriver {
    auto* orchestrator{get_first_actor<ATestBatchOrchestrator>(world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxTest, Fatal, TEXT("orchestrator is nullptr"));
    }

    return TestSimulationDriver{world, *orchestrator};
}

auto TestSimulationDriver::get_ledger() const -> ::ioj::sim::EntityLedger const& {
    auto const* const level{orchestrator.get_level_simulation()};
    check(level);
    return level->get_entity_ledger();
}

auto TestSimulationDriver::get_player_ship() const -> ATestSpaceShip const& {
    auto const actor{orchestrator.get_player_ship()};
    check(IsValid(actor));
    return *actor;
}
auto TestSimulationDriver::get_capital_ships() const -> ::ioj::sim::capital_ships::Sim const& {
    auto const* const level{orchestrator.get_level_simulation()};
    check(level);
    return level->get_capital_ships();
}
auto TestSimulationDriver::get_fighters() const -> ::ioj::sim::fighters::Sim const& {
    auto const* const level{orchestrator.get_level_simulation()};
    check(level);
    return level->get_fighters();
}

void TestSimulationDriver::queue_damage(std::span<sim::EntityId const> const targets,
                                        int32 const damage,
                                        sim::EntityId const instigator) {
    auto const n{static_cast<int32>(targets.size())};

    sim::DirectDamageEvents damage_events;
    damage_events.reserve(n);
    for (auto const target : targets) {
        damage_events.add(target, damage, instigator);
    }

    FTestBatchOrchestratorTestAccess::queue_direct_damage_events(orchestrator,
                                                                 damage_events.get_const_view());
}
void TestSimulationDriver::queue_kills(std::span<sim::EntityId const> const targets,
                                       sim::EntityId const instigator) {
    sim::DirectDamageEvents events;
    events.reserve(static_cast<int32>(targets.size()));
    for (auto const id : targets) {
        events.add(id, std::numeric_limits<int32>::max(), instigator);
    }
    FTestBatchOrchestratorTestAccess::queue_direct_damage_events(orchestrator,
                                                                 events.get_const_view());
}
bool TestSimulationDriver::should_export_results() const {
#if WITH_EDITOR
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    return settings->export_test_results;
#else
    return false;
#endif
}

void TestSimulationDriver::set_wait_until_tick_from_now(uint64 wait_cycles) {
    auto const* const level{orchestrator.get_level_simulation()};
    auto const completed_ticks{level ? level->get_clock().get_completed_ticks().value() : 0};
    tick_wait_end = completed_ticks + wait_cycles;
}
bool TestSimulationDriver::tick_wait_completed() const {
    auto const* const level{orchestrator.get_level_simulation()};
    auto const completed_ticks{level ? level->get_clock().get_completed_ticks().value() : 0};
    return completed_ticks >= tick_wait_end;
}

auto TestSimulationDriver::get_time() const -> time_type {
    auto const* const level{orchestrator.get_level_simulation()};
    return level ? level->get_clock().get_simulation_time() : 0.0;
}
void TestSimulationDriver::set_delta_time_wait(time_type dt) {
    check(dt > time_type{0});

    time_wait_end = get_time() + dt;
}
void TestSimulationDriver::set_time_wait(time_type wait_end) {
    check(wait_end > get_time());

    time_wait_end = wait_end;
}
bool TestSimulationDriver::time_wait_completed() const {
    return get_time() >= time_wait_end;
}
}
