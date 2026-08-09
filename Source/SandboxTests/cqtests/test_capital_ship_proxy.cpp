#include "test_setup.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <CQTest.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

TEST_CLASS(TestCapitalShipProxy, "Sandbox.FunctionalTests")
{
    ml::FTestBatchOrchestratorLevelSetup level_setup;
    int32 expected_health{0};

    BEFORE_EACH()
    {
        level_setup.setup(
            TestCommandBuilder,
            *TestRunner,
            [this](UWorld& world, UTestSimulationConfig const& config) {
                auto* const capital_config{config.simulation_config->capital_ships_config.Get()};
                check(capital_config);

                expected_health = capital_config->max_health;

                auto* const proxy{world.SpawnActorDeferred<ATestCapitalShipProxy>(
                    ATestCapitalShipProxy::StaticClass(), FTransform::Identity)};
                check(proxy);

                proxy->set_actor_config(capital_config);
                UGameplayStatics::FinishSpawningActor(proxy, FTransform::Identity);
            });
    }

    AFTER_EACH()
    { level_setup.teardown(); }

    TEST_METHOD(SpawnsWithConfiguredHealth)
    {
        TestCommandBuilder.Do([this] {
            auto test_driver{ml::TestSimulationDriver::from_world(level_setup.get_world())};
            auto const& capitals{test_driver.get_capital_ships()};

            if (!TestRunner->TestEqual(
                    TEXT("One capital ship is spawned from the proxy"),
                    capitals.get_num_instances(),
                    1)) {
                return;
            }

            auto const health{capitals.get_health(capitals.get_handle(0))};
            TestRunner->TestEqual(TEXT("Capital ship health matches its config"),
                                  health,
                                  expected_health);
        });
    }
};
