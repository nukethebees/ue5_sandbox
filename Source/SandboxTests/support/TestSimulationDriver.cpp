#include "TestSimulationDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>
#include <SandboxTests/support/SpaceGameTestSettings.h>

#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/PrimitiveComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <EngineUtils.h>

#include <limits>

namespace ml {
TestSimulationDriver::TestSimulationDriver(UWorld& world, ATestBatchOrchestrator& orchestrator)
    : world{world}
    , orchestrator{orchestrator} {
    time_scale = get_space_game_level_test_time_scale();
    orchestrator.set_time_scale(time_scale);
}

auto TestSimulationDriver::from_world(UWorld& world) -> TestSimulationDriver {
    auto* orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(world)};
    if (!IsValid(orchestrator)) {
        UE_LOG(LogSandboxTest, Fatal, TEXT("orchestrator is nullptr"));
    }

    return TestSimulationDriver{world, *orchestrator};
}

auto TestSimulationDriver::get_registry() const -> FTestEntityRegistry& {
    return orchestrator.get_entity_registry();
}

auto TestSimulationDriver::get_player_ship() const -> ATestSpaceShip const& {
    auto const actor{orchestrator.get_player_ship()};
    check(IsValid(actor));
    return *actor;
}
auto TestSimulationDriver::get_capital_ships() const -> test_capital_ships::Simulation const& {
    auto const simulation{orchestrator.get_capital_ships()};
    check(simulation);
    return *simulation;
}
auto TestSimulationDriver::get_capital_ship_fighters() const
    -> test_capital_ship_fighters::Simulation const& {
    auto const simulation{orchestrator.get_capital_ship_fighters()};
    check(simulation);
    return *simulation;
}

void TestSimulationDriver::queue_damage(TConstArrayView<FRegistryEntityHandle> const targets,
                                        int32 const damage,
                                        FRegistryEntityHandle const instigator) {
    auto const n{targets.Num()};

    DirectDamageEvents damage_events;
    damage_events.add_uninitialised(n);

    damage_events.damaged_entities = targets;
    ml::fill(damage_events.damage_amounts, damage);
    ml::fill(damage_events.instigators, instigator);

    get_registry().queue_direct_damage_events(damage_events);
}
void TestSimulationDriver::queue_kills(TConstArrayView<FRegistryEntityHandle> const targets,
                                       FRegistryEntityHandle const instigator) {
    queue_damage(targets, std::numeric_limits<int32>::max(), instigator);
}
bool TestSimulationDriver::should_export_results() const {
#if WITH_EDITOR
    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    return settings->export_test_results;
#else
    return false;
#endif
}

void TestSimulationDriver::set_time_scale(time_type const scale) {
    orchestrator.set_time_scale(scale);
}

void TestSimulationDriver::set_wait_until_tick_from_now(uint64 wait_cycles) {
    tick_wait_end = orchestrator.get_completed_ticks() + wait_cycles;
}
bool TestSimulationDriver::tick_wait_completed() const {
    return orchestrator.get_completed_ticks() >= tick_wait_end;
}

auto TestSimulationDriver::get_time() const -> time_type {
    return orchestrator.get_simulation_time();
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
