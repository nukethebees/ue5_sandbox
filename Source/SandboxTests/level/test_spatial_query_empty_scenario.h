#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/ProxyEntityMap.h>

namespace ml {
class FSpatialQueryEmptyScenario final : public FSimulationTestScenario {
    struct FSample {
        bool queries_completed{false};
        bool any_entity_is_null{false};
        int32 entities_in_range{INDEX_NONE};
    };
  public:
    explicit FSpatialQueryEmptyScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void run_queries();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_results();

    TOptional<TestSimulationDriver> driver{NullOpt};
    FSample current_sample{};
    TimeSeriesData<FSample> samples;
};

class FSpatialQueryRangeScenario final : public FSimulationTestScenario {
  public:
    explicit FSpatialQueryRangeScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void on_tear_down() override;
    void bind(FProxyEntityMap const& proxies);
    void run_query();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_results();

    TOptional<TestSimulationDriver> driver{NullOpt};
    FRegistryEntityHandle boundary_enemy;
    TArray<FRegistryEntityHandle> query_result;
    TimeSeriesData<TArray<FRegistryEntityHandle>> samples;
};
}
