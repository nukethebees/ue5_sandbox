#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
enum class ETurretAcquisitionRegressionScenario : uint8 {
    NoOtherEntity,
    FriendlyOnly,
    EnemyOutsideRadius,
};

class FTurretAcquisitionRegressionScenario final : public FSimulationTestScenario {
  public:
    FTurretAcquisitionRegressionScenario(FSimulationTestContext& context,
                                         ETurretAcquisitionRegressionScenario scenario);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_results();

    ETurretAcquisitionRegressionScenario scenario_;
    TOptional<TestSimulationDriver> driver{NullOpt};
    TimeSeriesData<TArray<FRegistryEntityHandle>> targets;
};
}
