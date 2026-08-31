#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FEntityInterfaceScenario final : public FSimulationTestScenario {
    static constexpr time_type test_time{0.25};
  public:
    explicit FEntityInterfaceScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void initial_setup();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_no_proxies_alive(int32 sample_index);
    void check_capital_targets(int32 sample_index);
    void main_checks();

    TimeSeriesData<int32> capital_proxy_counts;
    TimeSeriesData<int32> turret_proxy_counts;
    TimeSeriesData<int32> spinner_proxy_counts;
    TimeSeriesData<TArray<FRegistryEntityHandle>> capital_target_handles;
    TimeSeriesData<TArray<uint8>> capital_target_alive;
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};
};
}
