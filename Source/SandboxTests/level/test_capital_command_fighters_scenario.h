#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
void run_worldless_capital_command_fighters(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config);

class FCapitalCommandFightersScenario final : public FSimulationTestScenario {
    using Task = test_capital_ship_fighters::Simulation::Task;

    struct FSimulationSample {
        FRegistryEntityHandle capital_target;
        TArray<FRegistryEntityHandle> fighter_targets;
        TArray<Task> fighter_tasks;
        int32 capital_count{0};
    };

    static constexpr time_type wait_after_setup{2.0 / 60.0};
    static constexpr time_type wait_after_kills{2.0 / 60.0};
    static constexpr int32 test_capital_idx{0};
  public:
    explicit FCapitalCommandFightersScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_fixture();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void initial_setup_and_stimuli();

    template <auto EnumValue>
    void check_fighter_tasks_are(FSimulationSample const& sample);

    void check_target_handles(FRegistryEntityHandle capital_target,
                              FSimulationSample const& sample);
    void kill_initial_targets();
    void kill_all_not_on_main_team();
    void full_checks();

    test_capital_ships::Simulation const* capitals{nullptr};
    test_capital_ship_fighters::Simulation const* fighters{nullptr};
    FRegistryEntityHandle capital_first_target;
    FRegistryEntityHandle capital_second_target;
    int32 capital_fighter_start{0};
    int32 capital_fighter_end{0};
    ETestTeam team_kept_alive{ETestTeam::White};
    TimeSeriesData<FSimulationSample> samples;
    time_type t_after_setup{0.0};
    time_type t_after_initial_kills{0.0};
    time_type t_after_all_kills{0.0};
};
}
