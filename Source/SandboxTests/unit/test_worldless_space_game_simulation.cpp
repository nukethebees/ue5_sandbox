#include "../level/test_hud_manager_scenario.h"
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <CQTest.h>

TEST_CLASS(WorldlessSpaceGameSimulation, "Sandbox.UnitTests")
{
    ioj::FSoftTestAssertions checks{};
    USpaceGameLevelConfig const* config{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        config = ioj::get_default_level_config(checks);
    }
  private:
    template <typename Runner, typename... Args>
    void run(Runner && runner, Args && ... args) {
        if (config != nullptr) {
            runner(*TestRunner, checks, *config, Forward<Args>(args)...);
        }
        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }
  public:
    TEST_METHOD(HUD_InitialCachesPopulateWithoutHUD)
    {
        run(ioj::run_worldless_hud_manager_scenario,
            ioj::EHUDManagerScenario::InitialCachesPopulateWithoutHUD);
    }
    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    {
        run(ioj::run_worldless_hud_manager_scenario,
            ioj::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD);
    }
    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    {
        run(ioj::run_worldless_hud_manager_scenario,
            ioj::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    {
        run(ioj::run_worldless_hud_manager_scenario,
            ioj::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    {
        run(ioj::run_worldless_hud_manager_scenario,
            ioj::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD);
    }
};
