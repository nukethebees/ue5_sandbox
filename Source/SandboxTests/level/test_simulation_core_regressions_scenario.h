#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/entities/ProxyEntityMap.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
enum class ESimulationCoreRegressionScenario : uint8 { FixedTickLifecycle, DamageLifecycle };

void run_worldless_simulation_core_regression(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config,
                                              ESimulationCoreRegressionScenario scenario);

class FSimulationCoreRegressionScenario final : public FSimulationTestScenario {
    struct FDamageSample {
        int32 capital_count{0};
        int32 registry_alive_count{0};
        int32 health{0};
        int32 telemetry_active_count{0};
    };
  public:
    FSimulationCoreRegressionScenario(FSimulationTestContext& context,
                                      ESimulationCoreRegressionScenario scenario);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_damage_fixture();
    void bind_damage_fixture(FProxyEntityMap const& proxy_entities);
    void run_fixed_tick_lifecycle();
    void begin_damage_lifecycle();
    void on_damage_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_damage_lifecycle();

    ESimulationCoreRegressionScenario scenario_;
    TOptional<TestSimulationDriver> test_driver{NullOpt};
    TimeSeriesData<FDamageSample> damage_samples;
    FRegistryEntityHandle damaged_handle;
    int32 end_tick_calls{0};
};
}
