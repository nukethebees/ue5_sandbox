#include "TestSimulationDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
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

void TestSimulationDriver::queue_kills(AActor const& expected_hit,
                                       TConstArrayView<FRegistryEntityHandle> const targets) {
    // To avoid creating new code paths, we need to do a "collision" to do damage
    auto const damage{std::numeric_limits<int32>::max()};
    auto const n{targets.Num()};

    UnresolvedDamageEvents damage_events;
    damage_events.add_uninitialised(n);

    TArray<AActor const*> hit_actors;
    TMap<int32, int32> hit_items;

    for (int32 i{0}; i < n; ++i) {
        check(registry.is_valid_alive(targets[i]));
        FVector location{registry.get_location(targets[i])};
        FVector const half_trace{0.0, 0.0, 1.0};

        FHitResult hit;
        auto const did_hit{world.LineTraceSingleByChannel(
            hit,
            location - half_trace,
            location + half_trace,
            ml::collision::projectile,
            FCollisionQueryParams{SCENE_QUERY_STAT(TestDamageInjector), false})};

        check(did_hit);
        hit_actors.Add(hit.GetActor());

        damage_events.damaged_actors[i] = hit.GetActor();
        damage_events.damage_amounts[i] = damage;
        damage_events.actor_components[i] = hit.GetComponent();
        damage_events.hit_items[i] = hit.Item;
        damage_events.instigators[i] = {};

        if (hit_items.Contains(hit.Item)) {
            hit_items[hit.Item] += 1;
        } else {
            hit_items.Add(hit.Item, 1);
        }
    }

    FString err_msg;

    for (auto const* actor : hit_actors) {
        if (&expected_hit != actor) {
            err_msg += FString::Printf(TEXT("\nShould have hit %s (hit %s)"),
                                       *ml::get_best_display_name(expected_hit),
                                       *ml::get_best_display_name(*actor));
        }
    }

    for (auto [k, v] : hit_items) {
        if (v != 1) { err_msg += FString::Printf(TEXT("\nHit item %d hit %d times"), k, v); }
    }

    checkf(err_msg.IsEmpty(), TEXT("%s"), *err_msg);

    registry.queue_damage_events(damage_events);
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
