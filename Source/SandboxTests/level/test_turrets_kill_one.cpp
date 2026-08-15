#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
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

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_simple_batch"), TestRunner, checks); }
    AFTER_EACH()
    {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
  private:
    static constexpr time_type test_time{3.0};
    FTimespan const timeout{0, 0, 4};

    void sample_values(ATestBatchOrchestrator&) {
        auto const t{test_driver->get_time()};
        auto const& registry{test_driver->registry};

        unique_ids.add(t, registry.get_num_unique_ids_issued());
        kills.add(t, registry.count_kills());
        alive.add(t, registry.count_alive());

        auto const* turrets{test_driver->orchestrator.get_turrets()};
        check(turrets);
        target_handles.add(t, TArray<FRegistryEntityHandle>{turrets->get_target_handles()});
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        test_driver->timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        ml::reset_and_reserve_time_series(
            test_driver->orchestrator, test_time, unique_ids, kills, alive, target_handles);
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_time);
        test_driver->orchestrator.start_simulation();
    }

    void full_checks() {
        constexpr auto expected_kills{1};

        auto const test_i{kills.nearest_index(test_time)};
        auto const n_unique{unique_ids.value_at(test_i)};
        auto const n_kills{kills.value_at(test_i)};

        checks.is_greater_than(n_unique, int32{0}, TEXT("At least one unique id issued"));
        checks.are_equal(expected_kills, n_kills, TEXT("One kill"));
        checks.are_equal(
            n_unique - n_kills, alive.value_at(test_i), TEXT("Alive count matches kills"));

        auto const values{target_handles.value_at(test_i)};
        for (auto const& handle : values) {
            checks.is_true(handle.is_null(), TEXT("All handles end null"));
        }
    }

    TEST_METHOD(FuncT_simple_batch)
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
