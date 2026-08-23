#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/fixed_array.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <EngineUtils.h>

namespace {
using time_type = ml::TestSimulationDriver::time_type;

constexpr time_type test_time{3.0};
constexpr ETestTeam hero_team{ETestTeam::Blue};
constexpr ETestTeam enemy_team{ETestTeam::Red};
FTimespan const timeout{0, 0, 4};
ml::FTestBatchOrchestratorLevelSetup turret_los_blocking_level_setup{};
ml::FTestBatchOrchestratorLevelSetup turret_search_los_level_setup{};

struct FTurretTestState {
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    ml::TimeSeriesData<int32> unique_ids;
    ml::TimeSeriesData<int32> kills;
    ml::TimeSeriesData<int32> alive;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    ml::TimeSeriesData<TArray<int32>> turret_healths;

    TArray<FRegistryEntityHandle> turret_handles;
    TArray<ETestTeam> turret_teams;
    FRegistryEntityHandle enemy_handle;
    FDelegateHandle map_change_handle;
    bool turret_proxies_configured{false};
    bool proxy_entities_bound{false};
};

void reset_test_state(FTurretTestState& state, FAutomationTestBase& test_runner) {
    state.checks.test_runner = &test_runner;
    state.checks.all_passed = true;
    state.test_driver.Reset();
    state.turret_handles.Reset();
    state.turret_teams.Reset();
    state.enemy_handle = {};
    state.turret_proxies_configured = false;
    state.proxy_entities_bound = false;
}

template <typename ConfigureProxy>
void configure_turret_proxies(FTurretTestState& state,
                              uint32 const flags,
                              ConfigureProxy&& configure_proxy) {
    if (state.turret_proxies_configured || !(flags & MapChangeEventFlags::NewMap)) {
        return;
    }

    auto editor_world{ml::get_editor_world()};
    if (!editor_world) {
        state.checks.is_true(false, TEXT("Editor world is available"));
        return;
    }

    auto const proxies{ml::get_actors<ATestStaticTurretsProxy>(**editor_world)};
    for (auto* const proxy : proxies) {
        auto const team{proxy->get_team()};
        if (!state.checks.is_true((team == hero_team) || (team == enemy_team),
                                  TEXT("Turret proxy is on the hero or enemy team"))) {
            continue;
        }

        configure_proxy(*proxy, team);
    }

    state.turret_proxies_configured = true;
}

void bind_turret_proxy_entities(FTurretTestState& state, FProxyEntityMap const& proxy_entities) {
    for (auto const& [actor, identifiers] : proxy_entities) {
        auto const* const proxy{Cast<ATestStaticTurretsProxy>(actor)};
        if (!state.checks.is_valid(proxy, TEXT("Bound proxy is a static turret"))) {
            continue;
        }

        auto const team{proxy->get_team()};
        state.turret_handles.Add(identifiers.handle);
        state.turret_teams.Add(team);

        if (team == enemy_team) {
            state.checks.is_true(state.enemy_handle.is_null(), TEXT("One enemy turret is bound"));
            if (state.enemy_handle.is_null()) {
                state.enemy_handle = identifiers.handle;
            }
        }
    }

    state.proxy_entities_bound = true;
}

void sample_values(FTurretTestState& state, ATestBatchOrchestrator&) {
    auto const t{state.test_driver->get_time()};
    auto const& registry{state.test_driver->registry};

    state.unique_ids.add(t, registry.get_num_unique_ids_issued());
    state.kills.add(t, registry.count_kills());
    state.alive.add(t, registry.count_alive());

    auto const* turrets{state.test_driver->orchestrator.get_turrets()};
    check(turrets);
    state.target_handles.add(t, TArray<FRegistryEntityHandle>{turrets->get_target_handles()});

    TArray<int32> health_values;
    health_values.Reserve(state.turret_handles.Num());
    for (FRegistryEntityHandle const handle : state.turret_handles) {
        health_values.Add(registry.get_health(handle));
    }
    state.turret_healths.add(t, MoveTemp(health_values));
}

void sample_and_advance_timeline(FTurretTestState& state, ATestBatchOrchestrator& orchestrator) {
    sample_values(state, orchestrator);
    state.test_driver->timeline.tick(state.test_driver->get_time());
}

void check_initial_state(FTurretTestState& state) {
    state.checks.is_true(state.turret_proxies_configured, TEXT("Turret proxies are configured"));
    state.checks.is_true(state.proxy_entities_bound, TEXT("Turret proxy entities are bound"));
    state.checks.is_greater_than(
        state.turret_handles.Num(), int32{0}, TEXT("Turret handles are stored"));
    state.checks.are_equal(
        state.turret_handles.Num(), state.turret_teams.Num(), TEXT("Turret handles have teams"));
    state.checks.is_true(state.enemy_handle.is_valid(), TEXT("Enemy turret handle is stored"));
}

void begin_simulation(FTurretTestState& state, FOrchestratorEndTickTestHook hook) {
    state.test_driver = ml::TestSimulationDriver::from_world(state.spawner->GetWorld());
    state.test_driver->orchestrator.start_simulation();
    check_initial_state(state);

    if (!state.checks.all_passed) {
        return;
    }

    ml::reset_and_reserve_time_series(state.test_driver->orchestrator,
                                      test_time,
                                      state.unique_ids,
                                      state.kills,
                                      state.alive,
                                      state.target_handles,
                                      state.turret_healths);
    state.test_driver->orchestrator.set_end_tick_test_hook(MoveTemp(hook));
    state.test_driver->timeline.finish_at(test_time);
}

void teardown_test_state(FTurretTestState& state, void const* const listener) {
    if (state.map_change_handle.IsValid()) {
        FEditorDelegates::MapChange.Remove(state.map_change_handle);
        state.map_change_handle.Reset();
    }

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(listener);

    if (state.test_driver.IsSet()) {
        state.test_driver->orchestrator.clear_end_tick_test_hook();
        state.test_driver->orchestrator.pause_simulation();
    }
}
} // namespace

TEST_CLASS(TurretsKillOneTurret, "Sandbox.LevelTests")
{
    using ThisClass = TurretsKillOneTurret;

    FTurretTestState state{};
    int32 hero_health{100000};

    BEFORE_EACH()
    {
        reset_test_state(state, *TestRunner);
        state.map_change_handle = FEditorDelegates::MapChange.AddLambda(
            [this](uint32 const flags) { on_pre_begin_play(flags); });
        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &ThisClass::bind_proxy_entities);
        state.spawner = ml::level_test_setup(TEXT("FuncT_simple_batch"), TestRunner, state.checks);
    }
    AFTER_EACH()
    { teardown_test_state(state, this); }
  private:
    void on_pre_begin_play(uint32 const flags) {
        configure_turret_proxies(
            state, flags, [this](ATestStaticTurretsProxy& proxy, ETestTeam const team) {
                if (team == hero_team) {
                    proxy.set_health(hero_health);
                }
            });
    }
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
        bind_turret_proxy_entities(state, proxy_entities);
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_and_advance_timeline(state, orchestrator);
    }

    void initial_setup() {
        begin_simulation(state,
                         FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);
    }
    void full_checks() {
        auto const test_i{state.kills.nearest_index(test_time)};
        auto const n_unique{state.unique_ids.value_at(test_i)};
        auto const n_kills{state.kills.value_at(test_i)};

        state.checks.is_greater_than(n_unique, int32{0}, TEXT("At least one unique id issued"));
        state.checks.are_equal(
            n_unique - n_kills, state.alive.value_at(test_i), TEXT("Alive count matches kills"));

        state.checks.is_true(!state.turret_healths.is_empty(), TEXT("Turret healths are sampled"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);

        auto const& initial_healths{state.turret_healths.value_at(0)};
        state.checks.are_equal(state.turret_handles.Num(),
                               initial_healths.Num(),
                               TEXT("Turret health sample matches handles"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);

        auto const n_turrets{state.turret_handles.Num()};
        for (int32 i{}; i < n_turrets; ++i) {
            if (state.turret_teams[i] != hero_team) {
                continue;
            }

            state.checks.are_equal(hero_health, initial_healths[i], TEXT("Hero turret health"), i);
        }

        state.checks.is_true(state.test_driver->registry.is_valid_dead(state.enemy_handle),
                             TEXT("Enemy turret is dead"));

        auto const values{state.target_handles.value_at(test_i)};
        for (auto const& handle : values) {
            state.checks.is_true(handle.is_null(), TEXT("All handles end null"));
        }
    }

    TEST_METHOD(TurretsKillEnemy)
    {
        TestCommandBuilder
            .StartWhen([this] { return nullptr != state.spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return state.test_driver->timeline.is_finished(); }, timeout)
            .Then([this] {
                full_checks();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);
            });
    }
};

TEST_CLASS(ZeroDamageTurrets, "Sandbox.LevelTests")
{
    using ThisClass = ZeroDamageTurrets;

    FTurretTestState state{};

    BEFORE_EACH()
    {
        reset_test_state(state, *TestRunner);
        state.map_change_handle = FEditorDelegates::MapChange.AddLambda(
            [this](uint32 const flags) { on_pre_begin_play(flags); });
        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &ThisClass::bind_proxy_entities);
        state.spawner = ml::level_test_setup(TEXT("FuncT_simple_batch"), TestRunner, state.checks);
    }
    AFTER_EACH()
    { teardown_test_state(state, this); }
  private:
    void on_pre_begin_play(uint32 const flags) {
        configure_turret_proxies(state, flags, [](ATestStaticTurretsProxy& proxy, ETestTeam) {
            proxy.set_laser_damage(0);
        });
    }
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
        bind_turret_proxy_entities(state, proxy_entities);
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_and_advance_timeline(state, orchestrator);
    }

    void initial_setup() {
        begin_simulation(state,
                         FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);
    }
    void full_checks() {
        state.checks.is_true(!state.turret_healths.is_empty(), TEXT("Turret healths are sampled"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);

        auto const& initial_healths{state.turret_healths.value_at(0)};
        auto const n_turrets{state.turret_handles.Num()};
        state.checks.are_equal(
            n_turrets, initial_healths.Num(), TEXT("Turret health sample matches handles"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);

        auto const n_samples{state.turret_healths.num()};
        for (int32 sample_index{}; sample_index < n_samples; ++sample_index) {
            auto const& healths{state.turret_healths.value_at(sample_index)};
            state.checks.are_equal(n_turrets,
                                   healths.Num(),
                                   TEXT("Turret health sample matches handles"),
                                   sample_index);
            if (healths.Num() != n_turrets) {
                continue;
            }

            for (int32 turret_index{}; turret_index < n_turrets; ++turret_index) {
                state.checks.are_equal(initial_healths[turret_index],
                                       healths[turret_index],
                                       TEXT("Turret health does not change"),
                                       turret_index);
            }
        }

        for (FRegistryEntityHandle const handle : state.turret_handles) {
            state.checks.is_true(state.test_driver->registry.is_valid_alive(handle),
                                 TEXT("Turret is alive at the end"));
        }
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder
            .StartWhen([this] { return nullptr != state.spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return state.test_driver->timeline.is_finished(); }, timeout)
            .Then([this] {
                full_checks();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(state.checks);
            });
    }
};

TEST_CLASS(TurretLineOfSightBlocking, "Sandbox.LevelTests")
{
    using ThisClass = TurretLineOfSightBlocking;
    using time_type = ml::TestSimulationDriver::time_type;

    static constexpr time_type initial_enemy_check_time{1.0};
    static constexpr time_type blocker_scheduled_spawn_time{2.0};
    static constexpr time_type blocker_grace_period{0.2};
    static constexpr time_type test_end_time{4.0};
    static constexpr int32 turret_count{2};

    struct FTurretInfo {
        FVector location;
        ETestTeam team;
    };

    FTimespan const timeout{0, 0, 4};
    ml::TFixedArray<FTurretInfo, turret_count> const turret_infos{
        {{-5000.f, 0.f, 0.f}, ETestTeam::Blue},
        {{5000.f, 0.f, 0.f}, ETestTeam::Red},
    };

    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::TimeSeriesData<int32> laser_counts;
    ml::TimeSeriesData<int32> entity_counts;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    ml::TimeSeriesData<TArray<FVector3f>> registry_locations;
    TOptional<time_type> blocker_spawn_time{NullOpt};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        test_driver.Reset();
        blocker_spawn_time.Reset();

        turret_los_blocking_level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
        TestCommandBuilder.Do([this] {
            auto& world{turret_los_blocking_level_setup.get_world()};
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
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }

        turret_los_blocking_level_setup.end_test();
    }
    AFTER_ALL()
    {
        turret_los_blocking_level_setup.teardown();
    }
  private:
    void spawn_line_of_sight_blocker() {
        auto* const blocker{ml::spawn_visibility_blocker(
            *test_driver->get_world(), FTransform::Identity, TEXT("line_of_sight_blocker"))};
        if (!checks.is_valid(blocker, TEXT("Line-of-sight blocker is spawned"))) {
            return;
        }

        blocker_spawn_time = test_driver->get_time();
    }
    void sample_laser_count(ATestBatchOrchestrator & orchestrator) {
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
            simulation_time,
            ml::to_vector3f_array(test_driver->registry.get_entity_data().locations));
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_laser_count(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        test_driver =
            ml::TestSimulationDriver::from_world(turret_los_blocking_level_setup.get_world());
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        ml::reset_and_reserve_time_series(test_driver->orchestrator,
                                          test_end_time,
                                          laser_counts,
                                          entity_counts,
                                          target_handles,
                                          registry_locations);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline
            .at(blocker_scheduled_spawn_time, [this] { spawn_line_of_sight_blocker(); })
            .finish_at(test_end_time);
        test_driver->orchestrator.start_simulation();
    }

    void full_checks() {
        checks.is_true(blocker_spawn_time.IsSet(), TEXT("Line-of-sight blocker is spawned"));
        checks.is_true(!laser_counts.is_empty(), TEXT("Laser counts are sampled"));
        checks.is_true(!entity_counts.is_empty(), TEXT("Entity counts are sampled"));
        checks.is_true(!target_handles.is_empty(), TEXT("Turret target handles are sampled"));
        checks.is_true(!registry_locations.is_empty(), TEXT("Registry locations are sampled"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const* const lasers{test_driver->orchestrator.get_lasers()};
        checks.is_greater_than(lasers->get_number_spawned(), 0, TEXT("Lasers were fired"));

        auto const target_check_sample_index{
            target_handles.nearest_index(initial_enemy_check_time)};
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
        auto const blocker_effective_sample_index{
            laser_counts.nearest_index(blocker_effective_time)};
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

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.Do([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] {
                full_checks();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            });
    }
};

TEST_CLASS(TurretSearchRequiresLineOfSight, "Sandbox.LevelTests")
{
    using ThisClass = TurretSearchRequiresLineOfSight;
    using time_type = ml::TestSimulationDriver::time_type;

    static constexpr time_type test_end_time{1.0};
    FTimespan const timeout{0, 0, 2};
    FName const blue_turret_name{TEXT("BlueTurret")};
    FName const blocked_enemy_name{TEXT("BlockedEnemy")};
    FName const visible_enemy_name{TEXT("VisibleEnemy")};

    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    FRegistryEntityHandle blocked_enemy_handle;
    FRegistryEntityHandle visible_enemy_handle;

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        test_driver.Reset();
        blocked_enemy_handle.reset();
        visible_enemy_handle.reset();

        turret_search_los_level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
        TestCommandBuilder.Do([this] {
            auto& world{turret_search_los_level_setup.get_world()};
            ml::spawn_actors<ATestStaticTurretsProxy, 3>(
                world, [this](ATestStaticTurretsProxy& actor, int32 const i, ESpawnPhase const phase) {
                    if (phase == ESpawnPhase::PreSpawn) {
                        actor.set_laser_damage(0);
                        actor.set_test_name(i == 0 ? blue_turret_name
                                                   : (i == 1 ? blocked_enemy_name
                                                             : visible_enemy_name));
                        actor.set_team(i == 0 ? ETestTeam::Blue : ETestTeam::Red);
                        return;
                    }

                    actor.SetActorLocation(i == 0 ? FVector{-1000.f, 0.f, 0.f}
                                                  : (i == 1 ? FVector{1000.f, 0.f, 0.f}
                                                            : FVector{1000.f, 1000.f, 0.f}));
                });

            auto blocker_transform{FTransform::Identity};
            blocker_transform.SetScale3D(FVector{1.f, 0.2f, 1.f});
            auto* const blocker{
                ml::spawn_visibility_blocker(world, blocker_transform, TEXT("search_los_blocker"))};
            if (!checks.is_valid(blocker, TEXT("Line-of-sight blocker is spawned"))) {
                return;
            }

            ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this, &ThisClass::bind);
        });
    }
    AFTER_EACH()
    {
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }
        turret_search_los_level_setup.end_test();
    }
    AFTER_ALL()
    {
        turret_search_los_level_setup.teardown();
    }
  private:
    void bind(FProxyEntityMap const& proxy_entities) {
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
    void on_end_tick(ATestBatchOrchestrator& orchestrator) {
        auto const* const turrets{orchestrator.get_turrets()};
        check(turrets);
        target_handles.add(test_driver->get_time(),
                           TArray<FRegistryEntityHandle>{turrets->get_target_handles()});
        test_driver->timeline.tick(test_driver->get_time());
    }
    void initial_setup() {
        test_driver =
            ml::TestSimulationDriver::from_world(turret_search_los_level_setup.get_world());
        test_driver->orchestrator.start_simulation();
        checks.is_true(blocked_enemy_handle.is_valid(), TEXT("Blocked enemy handle is bound"));
        checks.is_true(visible_enemy_handle.is_valid(), TEXT("Visible enemy handle is bound"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        ml::reset_and_reserve_time_series(
            test_driver->orchestrator, test_end_time, target_handles);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_end_time);
    }
    void full_checks() {
        checks.is_true(!target_handles.is_empty(), TEXT("Turret target handles are sampled"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const sample_index{target_handles.nearest_index(test_end_time)};
        auto const& selected_targets{target_handles.value_at(sample_index)};
        checks.is_true(selected_targets.Contains(visible_enemy_handle),
                       TEXT("Visible enemy is selected as a target"));
        checks.is_true(!selected_targets.Contains(blocked_enemy_handle),
                       TEXT("Blocked enemy is not selected as a target"));
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.Do([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] {
                full_checks();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            });
    }
};
