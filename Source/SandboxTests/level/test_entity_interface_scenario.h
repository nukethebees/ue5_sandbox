#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FEntityInterfaceScenario final : public FSimulationTestScenario {
    using time_type = TestSimulationDriver::time_type;
    static constexpr time_type test_time{0.25};
  public:
    explicit FEntityInterfaceScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void spawn_fixture();
    void initial_setup();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_no_proxies_alive(int32 sample_index);
    void check_capital_targets(int32 sample_index);
    void main_checks();

    TOptional<TestSimulationDriver> test_driver{NullOpt};
    TimeSeriesData<int32> capital_proxy_counts;
    TimeSeriesData<int32> turret_proxy_counts;
    TimeSeriesData<int32> spinner_proxy_counts;
    TimeSeriesData<TArray<FRegistryEntityHandle>> capital_target_handles;
    TimeSeriesData<TArray<uint8>> capital_target_alive;
};
}
