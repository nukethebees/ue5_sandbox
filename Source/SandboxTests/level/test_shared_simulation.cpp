#include "test_batch_orchestrator_reset_scenario.h"
#include "test_batch_orchestrator_setup_scenario.h"
#include "test_capital_ship_proxy_scenario.h"
#include "test_entity_interface_scenario.h"
#include "test_hud_manager_scenario.h"
#include "test_level_loader_scenario.h"
#include "test_player_ship_death_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SpaceGameTestSettings.h>
#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>

#define SHARED_SIMULATION_TEST(METHOD_NAME, SCENARIO_TYPE, ...) \
    TEST_METHOD(METHOD_NAME)                                    \
    { run_scenario<SCENARIO_TYPE>(__VA_ARGS__); }

TEST_CLASS(SharedSimulation, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};
    TUniquePtr<ml::FSimulationTestContext> context{nullptr};
    TUniquePtr<ml::FSimulationTestScenario> scenario{nullptr};
    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (scenario) {
            scenario->tear_down();
        }
        scenario.Reset();
        context.Reset();
        level_setup.end_test();
    }

    AFTER_ALL()
    { level_setup.teardown(); }
  private:
    template <typename T, typename... Args>
    void run_scenario(Args && ... args) {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Shared simulation orchestrator is available"))) {
            return;
        }

        context = MakeUnique<ml::FSimulationTestContext>(ml::FSimulationTestContext{
            .orchestrator = *orchestrator,
            .automation_test = *TestRunner,
            .command_builder = TestCommandBuilder,
            .checks = checks,
            .config = level_setup.get_config(),
            .world = level_setup.get_world(),
            .level_construction_count = level_setup.get_construction_count(),
        });
        scenario = MakeUnique<T>(*context, Forward<Args>(args)...);
        check(scenario);
        scenario->run();
    }
  public:
    SHARED_SIMULATION_TEST(Orchestrator_SpawnMissingActors,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SpawnMissingActors)
    SHARED_SIMULATION_TEST(Orchestrator_SimulationClockConversions,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SimulationClockConversions)
    SHARED_SIMULATION_TEST(Orchestrator_LevelTelemetry,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::LevelTelemetry)
    SHARED_SIMULATION_TEST(Orchestrator_PresentationFrameOrdering,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::PresentationFrameOrdering)
    SHARED_SIMULATION_TEST(Orchestrator_ResetForNewLevel, ml::FTestBatchOrchestratorResetScenario)
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesDefinition, ml::FLevelLoaderScenario)
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesPlayerlessCameraDefinition,
                           ml::FLevelLoaderCameraScenario)

    SHARED_SIMULATION_TEST(CapitalShipProxy_HealthOverridesConfig,
                           ml::FTestCapitalShipProxyScenario)
    SHARED_SIMULATION_TEST(EntityInterface_ConvertsProxiesAndResolvesTargets,
                           ml::FEntityInterfaceScenario)
    SHARED_SIMULATION_TEST(HUD_LateRegistrationSynchronisesAndUnregisters,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters)
    SHARED_SIMULATION_TEST(PlayerShip_LethalDamageDestroysPlayerShip,
                           ml::FTestPlayerShipDeathScenario)
};

#undef SHARED_SIMULATION_TEST
