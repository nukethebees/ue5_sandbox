#include "../level/test_hud_manager_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <CQTest.h>

TEST_CLASS(WorldlessSpaceGameSimulation, "Sandbox.UnitTests")
{
    ml::FSoftTestAssertions checks{};
    USpaceGameLevelConfig const* config{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        config = ml::get_default_level_config(checks);
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
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::InitialCachesPopulateWithoutHUD);
    }
    TEST_METHOD(HUD_EntityCountPollingContinuesWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::EntityCountPollingContinuesWithoutHUD);
    }
    TEST_METHOD(HUD_MissionAndDefenceDataUpdateWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::MissionAndDefenceDataUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_PlayerStateAndKillsUpdateWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::PlayerStateAndKillsUpdateWithoutHUD);
    }
    TEST_METHOD(HUD_MissionTimeUsesSimulationClockWithoutHUD)
    {
        run(ml::run_worldless_hud_manager_scenario,
            ml::EHUDManagerScenario::MissionTimeUsesSimulationClockWithoutHUD);
    }
};
