#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>

#include <SandboxTests/cqtests/level_checks.h>
#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(PlayerShipVsCapital, "Sandbox.FunctionalTests")
{
    using time_type = ml::TestSimulationDriver::time_type;

    inline static FTimespan const default_timeout{0, 0, 1};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_player_ship_vs_capital"), TestRunner, checks); }
  private:
    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{1.0};

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
    }
    void initial_samples() {}
    void initial_checks() {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void initial_phase() {
        initial_setup();
        initial_samples();
        initial_checks();

        test_driver->set_delta_time_wait(initial_wait);
    }

    TEST_METHOD(Main)
    {
        auto check_wait_over{[this] { return test_driver->time_wait_completed(); }};

        TestCommandBuilder
            // Initial phase
            .Do([this] { initial_phase(); })
            .Until(check_wait_over, default_timeout);
    }
};
