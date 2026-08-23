#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

namespace ml {
enum class EOrchestratorSetupScenario : uint8 { SpawnMissingActors, SimulationClockConversions };
enum class EHUDManagerScenario : uint8 {
    InitialCachesPopulateWithoutHUD,
    EntityCountPollingContinuesWithoutHUD,
    MissionAndDefenceDataUpdateWithoutHUD,
    PlayerStateAndKillsUpdateWithoutHUD,
    MissionTimeUsesSimulationClockWithoutHUD,
    LateHUDRegistrationSynchronisesAndUnregisters,
};
enum class EMissionManagerScenario : uint8 {
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
    DefenceObjective,
};

auto make_orchestrator_setup_scenario(FSimulationTestContext& context,
                                      EOrchestratorSetupScenario scenario)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_orchestrator_reset_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_capital_ship_proxy_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_fighters_standby_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_hud_manager_scenario(FSimulationTestContext& context, EHUDManagerScenario scenario)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_mission_manager_scenario(FSimulationTestContext& context,
                                   EMissionManagerScenario scenario)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_player_ship_death_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_spatial_query_line_of_sight_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_turret_line_of_sight_blocking_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
auto make_turret_search_line_of_sight_scenario(FSimulationTestContext& context)
    -> TUniquePtr<FSimulationTestScenario>;
}
