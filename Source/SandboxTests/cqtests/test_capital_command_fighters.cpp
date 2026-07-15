#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <Sandbox/utilities/enums.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(CapitalCommandFighters, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::FSoftTestAssertions<std::remove_cvref_t<decltype(*TestRunner)>> checks{};

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    static constexpr int32 wait_after_setup{2};
    static constexpr int32 wait_after_kills{2};

    FRegistryEntityHandle capital_first_target;
    FRegistryEntityHandle capital_second_target;

    ETestTeam team_kept_alive;
    static constexpr int32 test_capital_idx{0};

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_capital_command_fighters"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);

        checks.test_runner = TestRunner;
        checks.all_passed = true;
#if WITH_EDITOR
        auto const* settings{GetDefault<USandboxDeveloperSettings>()};
        checks.log_successful_assertions = settings->log_successful_assertions;
#endif
    }
  private:
    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);

        capitals = &test_driver->get_capital_ships();
        fighters = &test_driver->get_capital_ship_fighters();

        capital_first_target = capitals->get_target_handle(test_capital_idx);

        test_driver->set_wait_until_tick_from_now(wait_after_setup);

        team_kept_alive = capitals->get_team(test_capital_idx);
    }

    template <auto EnumValue>
    void check_fighter_tasks_are() {
        auto const tasks{fighters->get_tasks()};
        auto const n{tasks.Num()};

        for (int32 i{0}; i < n; ++i) {
            checks.are_equal(EnumValue,
                             tasks[i],
                             FString::Printf(TEXT("Check fighter %d is in %s"),
                                             i,
                                             *ml::to_string_without_type_prefix(EnumValue)));
        }
    }

    void check_target_handles(FRegistryEntityHandle const capital_target) {
        checks.are_equal(
            capital_target, capitals->get_target_handle(0), TEXT("Capital handle hasn't changed"));

        auto const capital_fighter_span{capitals->get_capital_fighter_handle_span(0)};
        auto const fighter_targets{fighters->get_target_handles()};

        auto const end{capital_fighter_span.end()};
        for (int32 i{capital_fighter_span.start()}; i < end; ++i) {
            checks.are_equal(capital_target,
                             fighter_targets[i],
                             FString::Printf(TEXT("Fighter target handles [%d]"), i));
        }
    }

    void run_spawn_capital_handle_checks() {
        check_target_handles(capital_first_target);
        check_fighter_tasks_are<ATestCapitalShipFighters::Tasks::Attack>();
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    void kill_initial_targets() {
        test_driver->queue_kill(capitals->get_target_handle(test_capital_idx), {});

        test_driver->set_wait_until_tick_from_now(wait_after_kills);
    }
    void check_targets_after_kills() {
        capital_second_target = capitals->get_target_handles()[0];

        checks.is_true(capital_first_target != capital_second_target,
                       TEXT("Capital handles should be different"));

        check_target_handles(capital_second_target);
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    void kill_all_not_on_main_team() {
        auto const handles{test_driver->registry.get_handles_not_in_team(team_kept_alive)};
        for (auto const handle : handles) {
            test_driver->queue_kill(handle, {});
        }

        test_driver->set_wait_until_tick_from_now(wait_after_kills);
    }
    void check_fighters_after_main_enemies_killed() {
        check_fighter_tasks_are<ATestCapitalShipFighters::Tasks::Idle>();
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return test_driver->wait_is_over(); }, FTimespan{0, 0, 1})
            .Then([this] { run_spawn_capital_handle_checks(); })
            .Then([this] { kill_initial_targets(); })
            .Until([this] { return test_driver->wait_is_over(); }, FTimespan{0, 0, 1})
            .Then([this] { check_targets_after_kills(); })
            .Then([this] { kill_all_not_on_main_team(); })
            .Until([this] { return test_driver->wait_is_over(); }, FTimespan{0, 0, 1})
            .Then([this] { check_fighters_after_main_enemies_killed(); });
    }
};
