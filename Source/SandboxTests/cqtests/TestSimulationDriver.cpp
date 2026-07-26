#include "TestSimulationDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <Sandbox/batch_game/test_entity_registry/DirectDamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/constants/collision_channels.h>
#include <Sandbox/utilities/actor_utils.h>

#include <Components/PrimitiveComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <EngineUtils.h>

#include <limits>

namespace ml {
auto TestSimulationDriver::from_world(UWorld& world) -> TestSimulationDriver {
    ATestBatchOrchestrator* orchestrator{nullptr};
    ATestEntityRegistry* registry{nullptr};

    for (TActorIterator<ATestBatchOrchestrator> it(&world); it; ++it) {
        orchestrator = *it;
        break;
    }
    if (!IsValid(orchestrator)) { UE_LOG(LogSandboxTest, Fatal, TEXT("orchestrator is nullptr")); }

    registry = orchestrator->get_entity_registry();
    if (!IsValid(registry)) { UE_LOG(LogSandboxTest, Fatal, TEXT("registry is nullptr")); }

    return TestSimulationDriver{world, *registry, *orchestrator};
}

auto TestSimulationDriver::get_capital_ships() const -> ATestCapitalShips const& {
    return *orchestrator.get_capital_ships();
}
auto TestSimulationDriver::get_capital_ship_fighters() const -> ATestCapitalShipFighters const& {
    return *orchestrator.get_capital_ship_fighters();
}

void TestSimulationDriver::queue_kills(TConstArrayView<FRegistryEntityHandle> const targets) {
    auto const damage{std::numeric_limits<int32>::max()};
    auto const n{targets.Num()};

    DirectDamageEvents damage_events;
    damage_events.add_uninitialised(n);

    damage_events.damaged_entities = targets;
    ml::fill(damage_events.damage_amounts, damage);
    ml::fill(damage_events.instigators, FRegistryEntityHandle{});

    registry.queue_direct_damage_events(damage_events);
}

void TestSimulationDriver::set_wait_until_tick_from_now(uint64 wait_cycles) {
    tick_wait_end = orchestrator.get_tick_count() + wait_cycles;
}
bool TestSimulationDriver::tick_wait_completed() const {
    return orchestrator.get_tick_count() >= tick_wait_end;
}

auto TestSimulationDriver::get_time() const -> time_type {
    return world.GetTimeSeconds();
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
