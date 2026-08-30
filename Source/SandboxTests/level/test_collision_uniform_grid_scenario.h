#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/ProxyEntityMap.h>

namespace ml {
class FCollisionUniformGridScenario final : public FSimulationTestScenario {
    struct FSample {
        TArray<int32> expected_cell_counts;
        TArray<int32> found_cell_counts;
    };

    static constexpr time_type sample_time{0.1};
    inline static FTimespan const timeout{0, 0, 4};
  public:
    explicit FCollisionUniformGridScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void bind_proxy_entities(FProxyEntityMap const& proxies);
    void initialise_simulation();
    void sample_grid();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_results();

    TStaticArray<FRegistryEntityHandle, 5> expected_handles_{};
    TimeSeriesData<FSample> samples_;
};
}
