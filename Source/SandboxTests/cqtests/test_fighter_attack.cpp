#include "lex_to_string.h"
#include "TestFighterAttackDriver.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <Sandbox/utilities/world.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(FighterCapitalAttack, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ATestFighterAttackDriver* local_driver{nullptr};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_fighter_attack"), TestRunner, checks); }
  private:
    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        local_driver = ml::get_first_actor<ATestFighterAttackDriver>(world);
    }
    void initial_checks() {
        checks.is_valid(local_driver, TEXT("Local driver is valid."));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    inline static FTimespan const default_timeout{0, 0, 2};
    TEST_METHOD(Main)
    {
        TestCommandBuilder.Do([this] {
            initial_setup();
            initial_checks();
        });
    }
};
