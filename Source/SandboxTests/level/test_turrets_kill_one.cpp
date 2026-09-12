#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_turrets_kill_one.h"

#include <SandboxCore/fixed_array.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Containers/Set.h>
#include <Engine/World.h>

namespace ml {
namespace {
void add_worldless_turrets(FLevelSimulationInitData& data,
                           TConstArrayView<FVector3f> const locations,
                           TConstArrayView<ETestTeam> const teams,
                           int32 const laser_damage,
                           int32 const blue_health = INDEX_NONE) {
    check(locations.Num() == teams.Num());
    auto const count{locations.Num()};
    data.turret_spawns.add_defaulted(count);
    data.turret_transforms.SetNum(count);
    for (int32 i{}; i < count; ++i) {
        auto const health{blue_health != INDEX_NONE && teams[i] == ETestTeam::Blue
                              ? blue_health
                              : data.turrets.max_health};
        data.turret_spawns.locations.set(i, locations[i]);
        data.turret_spawns.teams[i] = teams[i];
        data.turret_spawns.healths[i] = health;
        data.turret_spawns.laser_damages[i] = laser_damage;
        data.turret_transforms[i].SetLocation(FVector{locations[i]});
    }
}
}

void run_worldless_turret_combat(FAutomationTestBase& test,
                                 FSoftTestAssertions& checks,
                                 USpaceGameLevelConfig const& config,
                                 ETurretCombatScenario const scenario) {
    static TArray<FVector3f> const locations{{3160.f, -3200.f, 40.f},
                                             {1710.f, -3200.f, 40.f},
                                             {190.f, -3200.f, 40.f},
                                             {-1050.f, -3200.f, 40.f},
                                             {-2380.f, -3200.f, 40.f},
                                             {-3550.f, -3200.f, 40.f},
                                             {190.f, 2280.f, 40.f}};
    static TArray<ETestTeam> const teams{ETestTeam::Blue,
                                         ETestTeam::Blue,
                                         ETestTeam::Blue,
                                         ETestTeam::Blue,
                                         ETestTeam::Blue,
                                         ETestTeam::Blue,
                                         ETestTeam::Red};
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    auto const damage{scenario == ETurretCombatScenario::ZeroDamage ? 0
                                                                    : data.turrets.laser.damage};
    add_worldless_turrets(data,
                          locations,
                          teams,
                          damage,
                          scenario == ETurretCombatScenario::KillEnemy ? 100000 : INDEX_NONE);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    TArray<int32> initial_healths;
    auto const& registry{harness.get_registry()};
    auto const initial_count{registry.get_num_elements()};
    initial_healths.Reserve(initial_count);
    for (int32 i{}; i < initial_count; ++i) {
        initial_healths.Add(registry.get_entity_data().healths[i]);
    }
    harness.timeline.finish_at(3.0);
    test.TestTrue(TEXT("Turret combat timeline completes"),
                  harness.run_until_timeline_finished(3.5));

    if (scenario == ETurretCombatScenario::KillEnemy) {
        checks.are_equal(1, registry.count_kills(), TEXT("One turret is killed"));
        checks.are_equal(6, registry.count_alive(), TEXT("Hero turrets remain alive"));
        for (auto const target : harness.get_simulation().get_turrets().get_target_handles()) {
            checks.is_true(target.is_null(), TEXT("Targets clear after the enemy dies"));
        }
        return;
    }

    checks.are_equal(
        initial_count, registry.count_alive(), TEXT("Zero-damage turrets remain alive"));
    for (int32 i{}; i < initial_count; ++i) {
        checks.are_equal(initial_healths[i],
                         registry.get_entity_data().healths[i],
                         TEXT("Zero-damage combat preserves health"),
                         i);
    }
}

void run_worldless_turret_line_of_sight_blocking(FAutomationTestBase& test,
                                                 FSoftTestAssertions& checks,
                                                 USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    TArray<FVector3f> const locations{{-5000.f, 0.f, 0.f}, {5000.f, 0.f, 0.f}};
    TArray<ETestTeam> const teams{ETestTeam::Blue, ETestTeam::Red};
    add_worldless_turrets(data, locations, teams, 0);
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    int32 spawn_count_before_blocker{INDEX_NONE};
    harness.timeline
        .at(2.0,
            [&] {
                spawn_count_before_blocker =
                    harness.get_simulation().get_lasers().get_number_spawned();
                harness.get_simulation()
                    .get_spatial_query_manager()
                    .get_collision_system()
                    .get_uniform_grid()
                    .add_static_aabb({-500.f, -3000.f, -3000.f}, {500.f, 3000.f, 3000.f});
            })
        .finish_at(4.0);
    test.TestTrue(TEXT("Turret line-of-sight timeline completes"),
                  harness.run_until_timeline_finished(4.5));
    checks.is_greater_than(spawn_count_before_blocker, 0, TEXT("Turrets fire before blocking"));
    checks.are_equal(spawn_count_before_blocker,
                     harness.get_simulation().get_lasers().get_number_spawned(),
                     TEXT("Turrets stop firing after line of sight is blocked"));
}

void run_worldless_turret_search_requires_line_of_sight(FAutomationTestBase& test,
                                                        FSoftTestAssertions& checks,
                                                        USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    TArray<FVector3f> const locations{
        {-1000.f, 0.f, 0.f}, {1000.f, 0.f, 0.f}, {1000.f, 1000.f, 0.f}};
    TArray<ETestTeam> const teams{ETestTeam::Blue, ETestTeam::Red, ETestTeam::Red};
    add_worldless_turrets(data, locations, teams, 0);
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    harness.get_simulation()
        .get_spatial_query_manager()
        .get_collision_system()
        .get_uniform_grid()
        .add_static_aabb({-100.f, -300.f, -3000.f}, {100.f, 300.f, 3000.f});
    harness.timeline.finish_at(1.0);
    test.TestTrue(TEXT("Turret search timeline completes"),
                  harness.run_until_timeline_finished(1.5));
    auto const targets{harness.get_simulation().get_turrets().get_target_handles()};
    checks.are_equal(3, targets.Num(), TEXT("All turret targets are available"));
    if (targets.Num() != 3) {
        return;
    }
    checks.is_true(targets[0].is_valid(), TEXT("Blue turret selects a visible target"));
    if (targets[0].is_valid()) {
        checks.dist_zero(FVector3f{1000.f, 1000.f, 0.f},
                         harness.get_registry().get_location(targets[0]),
                         1.f,
                         TEXT("Blue turret skips the blocked enemy"));
    }
}

}
