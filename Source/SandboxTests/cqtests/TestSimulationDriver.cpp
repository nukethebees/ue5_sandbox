#include "TestSimulationDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/constants/collision_channels.h>

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

void TestSimulationDriver::queue_damage(FRegistryEntityHandle const target,
                                        int32 const damage,
                                        FRegistryEntityHandle const instigator) {
    // To avoid creating new code paths, we need to do a "collision" to do damage

    check(registry.is_valid_alive(target));
    FVector location{registry.get_location(target)};
    FVector const half_trace{0.0, 0.0, 1.0};

    FHitResult hit;
    auto const did_hit{world.LineTraceSingleByChannel(
        hit,
        location - half_trace,
        location + half_trace,
        ml::collision::projectile,
        FCollisionQueryParams{SCENE_QUERY_STAT(TestDamageInjector), false})};

    check(did_hit);

    UnresolvedDamageEvents damage_events;
    damage_events.add_uninitialised(1);

    damage_events.damaged_actors[0] = hit.GetActor();
    damage_events.damage_amounts[0] = damage;
    damage_events.actor_components[0] = hit.GetComponent();
    damage_events.hit_items[0] = hit.Item;
    damage_events.instigators[0] = instigator;

    registry.queue_damage_events(damage_events);
}
void TestSimulationDriver::queue_kill(FRegistryEntityHandle const target,
                                      FRegistryEntityHandle const instigator) {
    queue_damage(target, std::numeric_limits<int32>::max(), instigator);
}

void TestSimulationDriver::set_wait_until_tick_from_now(uint64 wait_cycles) {
    tick_wait_end = orchestrator.get_tick_count() + wait_cycles;
}
bool TestSimulationDriver::wait_is_over() const {
    return orchestrator.get_tick_count() >= tick_wait_end;
}
}
