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

TEST_CLASS(TurretsKillOneTurret, "Sandbox.LevelTests")
{
    using ThisClass = TurretsKillOneTurret;
    using time_type = ml::TestSimulationDriver::time_type;

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

    BEFORE_EACH()
    {
        turret_handles.Reset();
        turret_teams.Reset();
        enemy_handle = {};
        turret_proxies_configured = false;
        proxy_entities_bound = false;

        map_change_handle = FEditorDelegates::MapChange.AddLambda(
            [this](uint32 const flags) { configure_turret_proxies(flags); });
        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &ThisClass::bind_proxy_entities);
        spawner = ml::level_test_setup(TEXT("FuncT_simple_batch"), TestRunner, checks);
    }
    AFTER_EACH()
    {
        if (map_change_handle.IsValid()) {
            FEditorDelegates::MapChange.Remove(map_change_handle);
            map_change_handle.Reset();
        }

        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);

        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }
    }
  private:
    static constexpr time_type test_time{3.0};
    static constexpr ETestTeam hero_team{ETestTeam::Blue};
    static constexpr ETestTeam enemy_team{ETestTeam::Red};
    FTimespan const timeout{0, 0, 4};
    int32 hero_health{100000};

    void configure_turret_proxies(uint32 const flags) {
        if (turret_proxies_configured || !(flags & MapChangeEventFlags::NewMap)) {
            return;
        }

        auto editor_world{ml::get_editor_world()};
        if (!editor_world) {
            checks.is_true(false, TEXT("Editor world is available"));
            return;
        }

        auto const proxies{ml::get_actors<ATestStaticTurretsProxy>(**editor_world)};
        for (auto* const proxy : proxies) {
            auto const team{proxy->get_team()};
            if (team == hero_team) {
                proxy->set_health(hero_health);
                continue;
            }

            checks.is_true(team == enemy_team, TEXT("Turret proxy is on the hero or enemy team"));
        }

        turret_proxies_configured = true;
    }
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
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

    void sample_values(ATestBatchOrchestrator&) {
        auto const t{test_driver->get_time()};
        auto const& registry{test_driver->registry};

        unique_ids.add(t, registry.get_num_unique_ids_issued());
        kills.add(t, registry.count_kills());
        alive.add(t, registry.count_alive());

        auto const* turrets{test_driver->orchestrator.get_turrets()};
        check(turrets);
        target_handles.add(t, TArray<FRegistryEntityHandle>{turrets->get_target_handles()});

        TArray<int32> health_values;
        health_values.Reserve(turret_handles.Num());
        for (FRegistryEntityHandle const handle : turret_handles) {
            health_values.Add(registry.get_health(handle));
        }
        turret_healths.add(t, MoveTemp(health_values));
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        initial_checks();
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        ml::reset_and_reserve_time_series(test_driver->orchestrator,
                                          test_time,
                                          unique_ids,
                                          kills,
                                          alive,
                                          target_handles,
                                          turret_healths);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_time);
        test_driver->orchestrator.start_simulation();
    }

    void initial_checks() {
        checks.is_true(turret_proxies_configured, TEXT("Turret proxies are configured"));
        checks.is_true(proxy_entities_bound, TEXT("Turret proxy entities are bound"));
        checks.is_greater_than(turret_handles.Num(), int32{0}, TEXT("Turret handles are stored"));
        checks.are_equal(
            turret_handles.Num(), turret_teams.Num(), TEXT("Turret handles have teams"));
        checks.is_true(enemy_handle.is_valid(), TEXT("Enemy turret handle is stored"));
    }

    void full_checks() {
        auto const test_i{kills.nearest_index(test_time)};
        auto const n_unique{unique_ids.value_at(test_i)};
        auto const n_kills{kills.value_at(test_i)};

        checks.is_greater_than(n_unique, int32{0}, TEXT("At least one unique id issued"));
        checks.are_equal(
            n_unique - n_kills, alive.value_at(test_i), TEXT("Alive count matches kills"));

        checks.is_true(!turret_healths.is_empty(), TEXT("Turret healths are sampled"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const& initial_healths{turret_healths.value_at(0)};
        checks.are_equal(turret_handles.Num(),
                         initial_healths.Num(),
                         TEXT("Turret health sample matches handles"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const n_turrets{turret_handles.Num()};
        for (int32 i{}; i < n_turrets; ++i) {
            if (turret_teams[i] != hero_team) {
                continue;
            }

            checks.are_equal(hero_health, initial_healths[i], TEXT("Hero turret health"), i);
        }

        checks.is_true(test_driver->registry.is_valid_dead(enemy_handle),
                       TEXT("Enemy turret is dead"));

        auto const values{target_handles.value_at(test_i)};
        for (auto const& handle : values) {
            checks.is_true(handle.is_null(), TEXT("All handles end null"));
        }
    }

    TEST_METHOD(MainTest)
    {

        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] {
                full_checks();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
            });
    }
};
