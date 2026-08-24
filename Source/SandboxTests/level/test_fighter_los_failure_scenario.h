#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/entities/TestTeam.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FFighterLosFailureScenario final : public FSimulationTestScenario {
    struct FSimulationSample {
        TArray<ETestTeam> fighter_teams;
    };

    static constexpr double test_duration{30.0};
  public:
    explicit FFighterLosFailureScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void spawn_fixture();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void check_fighter_spawns_and_survival();
    void full_checks();

    TOptional<TestSimulationDriver> test_driver{NullOpt};
    FRegistryEntityHandle enemy;
    ETestTeam hero_team{ETestTeam::Blue};
    ETestTeam enemy_team{ETestTeam::Red};
    int32 initial_enemy_health{0};
    TimeSeriesData<FSimulationSample> samples;
};
}
