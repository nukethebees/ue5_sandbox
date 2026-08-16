#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Editor.h>
#include <EngineUtils.h>

namespace {
using time_type = ml::TestSimulationDriver::time_type;

constexpr time_type test_time{3.0};
constexpr ETestTeam hero_team{ETestTeam::Blue};
constexpr ETestTeam enemy_team{ETestTeam::Red};
FTimespan const timeout{0, 0, 4};

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
    state.test_driver->orchestrator.start_simulation();
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
