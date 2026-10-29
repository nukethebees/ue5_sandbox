#pragma once

#include <ioj/sim/entities/entity_type.h>
#include <SandboxTests/support/SimulationTestScenario.h>
#include <SpaceGamePresentation/entities/TestTeam.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>

namespace ioj {
enum class EHUDManagerScenario : uint8 {
    InitialCachesPopulateWithoutHUD,
    EntityCountPollingContinuesWithoutHUD,
    MissionAndDefenceDataUpdateWithoutHUD,
    PlayerStateAndKillsUpdateWithoutHUD,
    MissionTimeUsesSimulationClockWithoutHUD,
};

void run_worldless_hud_manager_scenario(FAutomationTestBase& test,
                                        FSoftTestAssertions& checks,
                                        USpaceGameLevelConfig const& config,
                                        EHUDManagerScenario scenario);

class FTestHUDManagerScenario final : public FSimulationTestScenario {
  public:
    explicit FTestHUDManagerScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void registration_process_samples();
    auto initialise_headless_hud_manager() -> bool;
    auto get_headless_hud_manager() -> ioj::FHUDManager&;
    void tick_headless_hud_manager();
    void check_headless_hud_manager_matches_simulation();
#if WITH_EDITOR
    void check_sampled_speed_data_equal(hud_manager::FSampledSpeedDataCache const& local,
                                        hud_manager::FSampledSpeedDataCache const& simulation);
#endif

    TOptional<ioj::FHUDManager> headless_hud_manager{NullOpt};
};
}
