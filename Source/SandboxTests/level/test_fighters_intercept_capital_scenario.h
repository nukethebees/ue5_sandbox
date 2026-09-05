#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
void run_worldless_fighters_intercept_capital(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config);

class FFightersInterceptCapitalScenario final : public FSimulationTestScenario {

    struct FSimulationSample {
        FRegistryEntityHandle parent_target;
        TArray<FRegistryEntityHandle> fighter_targets;
        int32 fighter_count{0};
    };

    static constexpr time_type test_duration{20.0};
    static constexpr time_type initial_setup_duration{2.0 / 60.0};
  public:
    explicit FFightersInterceptCapitalScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_fixture();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void check_fighters_share_target(FSimulationSample const& sample, FString const& description);
    void full_checks();
    void export_data() const;

    test_capital_ships::Simulation const* capitals{nullptr};
    test_capital_ship_fighters::Simulation const* fighters{nullptr};
    TimeSeriesData<FSimulationSample> samples;
    int32 hero_capital_index{INDEX_NONE};
    FRegistryEntityHandle hero_capital;
    FRegistryEntityHandle original_target;
    FRegistryEntityHandle intercept_target;
};
}
