#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/time_series_test_data.h>
#include "test_turret_combat_scenario.h"
#include "test_turret_line_of_sight_blocking_scenario.h"
#include "test_turret_search_requires_line_of_sight_scenario.h"

#include <SandboxCore/fixed_array.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Containers/Set.h>
#include <Engine/World.h>

namespace ml {
FTurretCombatScenario::FTurretCombatScenario(FSimulationTestContext& context,
                                             ETurretCombatScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FTurretCombatScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

/* ------------------------------------------------------------------------------------------ */
// Shared turret combat fixture
/* ------------------------------------------------------------------------------------------ */
void FTurretCombatScenario::spawn_fixture() {
    static TStaticArray<FVector, 7> const locations{
        FVector{3160.f, -3200.f, 40.f},
        FVector{1710.f, -3200.f, 40.f},
        FVector{190.f, -3200.f, 40.f},
        FVector{-1050.f, -3200.f, 40.f},
        FVector{-2380.f, -3200.f, 40.f},
        FVector{-3550.f, -3200.f, 40.f},
        FVector{190.f, 2280.f, 40.f},
    };

    auto* const simulation_config{context_.config.simulation_config.Get()};
    if (!checks.not_nullptr(simulation_config, TEXT("Simulation config is available"))) {
        return;
    }
    spawn_actors<ATestStaticTurretsProxy, 7>(
        context_.world,
        [this, simulation_config](
            ATestStaticTurretsProxy& actor, int32 const i, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(simulation_config->static_turrets_config);
                actor.set_team(i == locations.Num() - 1 ? enemy_team : hero_team);
                if (scenario_ == ETurretCombatScenario::KillEnemy && i < locations.Num() - 1) {
                    actor.set_health(hero_health);
                }
                if (scenario_ == ETurretCombatScenario::ZeroDamage) {
                    actor.set_laser_damage(0);
                }
                return;
            }
            actor.SetActorLocation(locations[i]);
        });
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FTurretCombatScenario::bind_proxy_entities);
}

void FTurretCombatScenario::bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
    for (auto const& [actor, identifiers] : proxy_entities) {
        auto const* const proxy{Cast<ATestStaticTurretsProxy>(actor)};
        if (!checks.is_valid(proxy, TEXT("Bound proxy is a static turret"))) {
            continue;
        }
        auto const team{proxy->get_team()};
        turret_handles.Add(identifiers.handle);
        turret_teams.Add(team);
        if (team == enemy_team) {
            checks.is_true(enemy_handle.is_null(), TEXT("One enemy turret is bound"));
            if (enemy_handle.is_null()) {
                enemy_handle = identifiers.handle;
            }
        }
    }
    proxy_entities_bound = true;
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FTurretCombatScenario::check_initial_state() {
    checks.is_true(proxy_entities_bound, TEXT("Turret proxy entities are bound"));
    checks.is_greater_than(turret_handles.Num(), int32{0}, TEXT("Turret handles are stored"));
    checks.are_equal(turret_handles.Num(), turret_teams.Num(), TEXT("Turret handles have teams"));
    checks.is_true(enemy_handle.is_valid(), TEXT("Enemy turret handle is stored"));
}

void FTurretCombatScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    check_initial_state();
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    reset_and_reserve_time_series(test_driver->orchestrator,
                                  test_time,
                                  unique_ids,
                                  kills,
                                  alive,
                                  target_handles,
                                  turret_healths);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FTurretCombatScenario::on_end_tick));
    test_driver->timeline.finish_at(test_time);
}

void FTurretCombatScenario::sample_values() {
    auto const time{test_driver->get_time()};
    auto const& registry{test_driver->registry};
    unique_ids.add(time, registry.get_num_unique_ids_issued());
    kills.add(time, registry.count_kills());
    alive.add(time, registry.count_alive());
    auto const* const turrets{test_driver->orchestrator.get_turrets()};
    check(turrets);
    TArray<FRegistryEntityHandle> targets;
    targets.Append(turrets->get_target_handles());
    target_handles.add(time, MoveTemp(targets));

    TArray<int32> health_values;
    health_values.Reserve(turret_handles.Num());
    for (auto const handle : turret_handles) {
        health_values.Add(registry.get_health(handle));
    }
    turret_healths.add(time, MoveTemp(health_values));
}

void FTurretCombatScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FTurretCombatScenario::check_kill_enemy_results() {
    auto const sample_index{kills.nearest_index(test_time)};
    auto const n_unique{unique_ids.value_at(sample_index)};
    auto const n_kills{kills.value_at(sample_index)};
    checks.is_greater_than(n_unique, int32{0}, TEXT("At least one unique id issued"));
    checks.are_equal(
        n_unique - n_kills, alive.value_at(sample_index), TEXT("Alive count matches kills"));
    checks.is_true(!turret_healths.is_empty(), TEXT("Turret healths are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& initial_healths{turret_healths.value_at(0)};
    checks.are_equal(
        turret_handles.Num(), initial_healths.Num(), TEXT("Turret health sample matches handles"));
    for (int32 i{0}; i < turret_handles.Num(); ++i) {
        if (turret_teams[i] == hero_team) {
            checks.are_equal(hero_health, initial_healths[i], TEXT("Hero turret health"), i);
        }
    }
    checks.is_true(test_driver->registry.is_valid_dead(enemy_handle), TEXT("Enemy turret is dead"));
    for (auto const handle : target_handles.value_at(sample_index)) {
        checks.is_true(handle.is_null(), TEXT("All handles end null"));
    }
}

void FTurretCombatScenario::check_zero_damage_results() {
    checks.is_true(!turret_healths.is_empty(), TEXT("Turret healths are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& initial_healths{turret_healths.value_at(0)};
    auto const n_turrets{turret_handles.Num()};
    checks.are_equal(
        n_turrets, initial_healths.Num(), TEXT("Turret health sample matches handles"));
    for (int32 sample_index{0}; sample_index < turret_healths.num(); ++sample_index) {
        auto const& healths{turret_healths.value_at(sample_index)};
        checks.are_equal(
            n_turrets, healths.Num(), TEXT("Turret health sample matches handles"), sample_index);
        if (healths.Num() != n_turrets) {
            continue;
        }
        for (int32 turret_index{0}; turret_index < n_turrets; ++turret_index) {
            checks.are_equal(initial_healths[turret_index],
                             healths[turret_index],
                             TEXT("Turret health does not change"),
                             turret_index);
        }
    }
    for (auto const handle : turret_handles) {
        checks.is_true(test_driver->registry.is_valid_alive(handle),
                       TEXT("Turret is alive at the end"));
    }
}

void FTurretCombatScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); },
                                timeout,
                                [this] {
                                    if (scenario_ == ETurretCombatScenario::KillEnemy) {
                                        check_kill_enemy_results();
                                    } else {
                                        check_zero_damage_results();
                                    }
                                    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                                });
}

FTurretLineOfSightBlockingScenario::FTurretLineOfSightBlockingScenario(
    FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] {
        auto& world{context_.world};
        ml::spawn_actors<ATestStaticTurretsProxy, turret_count>(
            world, [&](ATestStaticTurretsProxy& actor, int32 const i, ESpawnPhase const phase) {
                if (phase == ESpawnPhase::PreSpawn) {
                    actor.set_team(turret_infos[i].team);
                    actor.set_laser_damage(0);
                    return;
                }

                actor.SetActorLocation(turret_infos[i].location);
            });
    });
}

void FTurretLineOfSightBlockingScenario::spawn_line_of_sight_blocker() {
    auto* const blocker{ml::spawn_visibility_blocker(
        *test_driver->get_world(), FTransform::Identity, TEXT("line_of_sight_blocker"))};
    if (!checks.is_valid(blocker, TEXT("Line-of-sight blocker is spawned"))) {
        return;
    }

    blocker_spawn_time = test_driver->get_time();
}

void FTurretLineOfSightBlockingScenario::sample_laser_count(ATestBatchOrchestrator& orchestrator) {
    auto const* const lasers{orchestrator.get_lasers()};
    auto const* const turrets{orchestrator.get_turrets()};
    check(lasers);
    check(turrets);
    auto const simulation_time{test_driver->get_time()};
    laser_counts.add(simulation_time, lasers->get_num_instances());
    entity_counts.add(simulation_time, test_driver->registry.get_num_elements());
    target_handles.add(simulation_time,
                       TArray<FRegistryEntityHandle>{turrets->get_target_handles()});
    registry_locations.add(
        simulation_time, ml::to_vector3f_array(test_driver->registry.get_entity_data().locations));
}

void FTurretLineOfSightBlockingScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    sample_laser_count(orchestrator);
    test_driver->advance_timeline();
}

void FTurretLineOfSightBlockingScenario::initial_setup() {
    initialise_test_driver();
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    ml::reset_and_reserve_time_series(test_driver->orchestrator,
                                      test_end_time,
                                      laser_counts,
                                      entity_counts,
                                      target_handles,
                                      registry_locations);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTurretLineOfSightBlockingScenario::on_end_tick));
    test_driver->timeline
        .at(blocker_scheduled_spawn_time, [this] { spawn_line_of_sight_blocker(); })
        .finish_at(test_end_time);
    test_driver->orchestrator.start_simulation();
}

void FTurretLineOfSightBlockingScenario::full_checks() {
    checks.is_true(blocker_spawn_time.IsSet(), TEXT("Line-of-sight blocker is spawned"));
    checks.is_true(!laser_counts.is_empty(), TEXT("Laser counts are sampled"));
    checks.is_true(!entity_counts.is_empty(), TEXT("Entity counts are sampled"));
    checks.is_true(!target_handles.is_empty(), TEXT("Turret target handles are sampled"));
    checks.is_true(!registry_locations.is_empty(), TEXT("Registry locations are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const* const lasers{test_driver->orchestrator.get_lasers()};
    checks.is_greater_than(lasers->get_number_spawned(), 0, TEXT("Lasers were fired"));

    auto const target_check_sample_index{target_handles.nearest_index(initial_enemy_check_time)};
    auto const& target_check_handles{target_handles.value_at(target_check_sample_index)};
    checks.are_equal(2,
                     target_check_handles.Num(),
                     TEXT("Two turret target handles are sampled after 1 second"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    for (FRegistryEntityHandle const handle : target_check_handles) {
        checks.is_true(!handle.is_null(), TEXT("Turret has a target after 1 second"));
    }

    TSet<FVector3f> expected_turret_locations;
    for (FTurretInfo const& turret_info : turret_infos) {
        expected_turret_locations.Add(FVector3f{turret_info.location});
    }
    checks.are_equal(
        turret_count, expected_turret_locations.Num(), TEXT("Turret locations are distinct"));

    auto const registry_location_sample_index{
        registry_locations.nearest_index(initial_enemy_check_time)};
    auto const& sampled_registry_locations{
        registry_locations.value_at(registry_location_sample_index)};
    checks.are_equal(turret_count,
                     sampled_registry_locations.Num(),
                     TEXT("Two turret registry locations are sampled after 1 second"));

    TSet<FVector3f> actual_turret_locations;
    for (FVector3f const& location : sampled_registry_locations) {
        checks.is_true(expected_turret_locations.Contains(location),
                       TEXT("Registry location belongs to a spawned turret"));
        actual_turret_locations.Add(location);
    }
    checks.are_equal(expected_turret_locations.Num(),
                     actual_turret_locations.Num(),
                     TEXT("Registry locations match the spawned turrets"));

    auto const actual_blocker_spawn_time{blocker_spawn_time.GetValue()};
    auto fired_before_blocker{false};
    auto const n_samples{laser_counts.num()};
    for (int32 i{}; i < n_samples; ++i) {
        if ((laser_counts.time_at(i) < actual_blocker_spawn_time) &&
            (laser_counts.value_at(i) > 0)) {
            fired_before_blocker = true;
        }
    }
    checks.is_true(fired_before_blocker, TEXT("Turrets fire before the blocker is spawned"));

    auto const blocker_effective_time{actual_blocker_spawn_time + blocker_grace_period};
    auto const blocker_effective_sample_index{laser_counts.nearest_index(blocker_effective_time)};
    for (int32 i{blocker_effective_sample_index + 1}; i < n_samples; ++i) {
        checks.are_equal(
            0, laser_counts.value_at(i), TEXT("No lasers after line-of-sight is blocked"), i);
    }

    checks.are_equal(
        laser_counts.num(), entity_counts.num(), TEXT("Laser and entity samples match"));
    auto const n_entity_samples{entity_counts.num()};
    for (int32 i{}; i < n_entity_samples; ++i) {
        checks.are_equal(2, entity_counts.value_at(i), TEXT("Two turret entities"), i);
    }
}

void FTurretLineOfSightBlockingScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); },
                                timeout,
                                [this] {
                                    full_checks();
                                    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                                });
}

FTurretSearchRequiresLineOfSightScenario::FTurretSearchRequiresLineOfSightScenario(
    FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] {
        auto& world{context_.world};
        ml::spawn_actors<ATestStaticTurretsProxy, 3>(
            world, [this](ATestStaticTurretsProxy& actor, int32 const i, ESpawnPhase const phase) {
                if (phase == ESpawnPhase::PreSpawn) {
                    actor.set_laser_damage(0);
                    actor.set_test_name(i == 0
                                            ? blue_turret_name
                                            : (i == 1 ? blocked_enemy_name : visible_enemy_name));
                    actor.set_team(i == 0 ? ETestTeam::Blue : ETestTeam::Red);
                    return;
                }

                actor.SetActorLocation(
                    i == 0 ? FVector{-1000.f, 0.f, 0.f}
                           : (i == 1 ? FVector{1000.f, 0.f, 0.f} : FVector{1000.f, 1000.f, 0.f}));
            });

        auto blocker_transform{FTransform::Identity};
        blocker_transform.SetScale3D(FVector{1.f, 0.2f, 1.f});
        auto* const blocker{
            ml::spawn_visibility_blocker(world, blocker_transform, TEXT("search_los_blocker"))};
        if (!checks.is_valid(blocker, TEXT("Line-of-sight blocker is spawned"))) {
            return;
        }

        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
            this, &FTurretSearchRequiresLineOfSightScenario::bind);
    });
}

void FTurretSearchRequiresLineOfSightScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FTurretSearchRequiresLineOfSightScenario::bind(FProxyEntityMap const& proxy_entities) {
    for (auto const& [actor, identifiers] : proxy_entities) {
        auto const* const proxy{Cast<ATestStaticTurretsProxy>(actor)};
        if (!checks.is_valid(proxy, TEXT("Bound proxy is a static turret"))) {
            continue;
        }

        if (proxy->get_test_name() == blocked_enemy_name) {
            blocked_enemy_handle = proxy->get_entity_handle();
        } else if (proxy->get_test_name() == visible_enemy_name) {
            visible_enemy_handle = proxy->get_entity_handle();
        }
    }
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FTurretSearchRequiresLineOfSightScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    auto const* const turrets{orchestrator.get_turrets()};
    check(turrets);
    target_handles.add(test_driver->get_time(),
                       TArray<FRegistryEntityHandle>{turrets->get_target_handles()});
    test_driver->advance_timeline();
}

void FTurretSearchRequiresLineOfSightScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    checks.is_true(blocked_enemy_handle.is_valid(), TEXT("Blocked enemy handle is bound"));
    checks.is_true(visible_enemy_handle.is_valid(), TEXT("Visible enemy handle is bound"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    ml::reset_and_reserve_time_series(test_driver->orchestrator, test_end_time, target_handles);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FTurretSearchRequiresLineOfSightScenario::on_end_tick));
    test_driver->timeline.finish_at(test_end_time);
}

void FTurretSearchRequiresLineOfSightScenario::full_checks() {
    checks.is_true(!target_handles.is_empty(), TEXT("Turret target handles are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const sample_index{target_handles.nearest_index(test_end_time)};
    auto const& selected_targets{target_handles.value_at(sample_index)};
    checks.is_true(selected_targets.Contains(visible_enemy_handle),
                   TEXT("Visible enemy is selected as a target"));
    checks.is_true(!selected_targets.Contains(blocked_enemy_handle),
                   TEXT("Blocked enemy is not selected as a target"));
}

void FTurretSearchRequiresLineOfSightScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); },
                                timeout,
                                [this] {
                                    full_checks();
                                    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                                });
}
}
