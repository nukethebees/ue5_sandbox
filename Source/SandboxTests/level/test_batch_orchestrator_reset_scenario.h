#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>

class AActor;

namespace ml {
class FTestBatchOrchestratorResetScenario final : public FSimulationTestScenario {

    static constexpr int32 blocker_count{3};
    static constexpr time_type reset_time{2.0};
    static constexpr int32 owned_actor_count{7};
    static constexpr int32 max_transient_actor_count{64};

    struct FSimulationSample {
        int32 actor_count{0};
    };
  public:
    explicit FTestBatchOrchestratorResetScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_blockers(UWorld& world);
    auto count_actors(UWorld const& world) const -> int32;
    auto is_core_runtime_actor(AActor const& actor) const -> bool;
    void save_old_owned_actors(ATestBatchOrchestrator const& orchestrator);
    void save_old_transient_actors(ATestBatchOrchestrator const& orchestrator);
    void sample(ATestBatchOrchestrator& orchestrator);
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void start_initial_simulation();
    void reset_simulation();
    void check_reset();

    FTimespan const timeout{0, 0, 3};
    TimeSeriesData<FSimulationSample> initial_samples;
    TimeSeriesData<FSimulationSample> reset_samples;
    TStaticArray<TWeakObjectPtr<AActor>, blocker_count> blockers;
    TStaticArray<AActor*, owned_actor_count> old_owned_actors{};
    TStaticArray<AActor*, max_transient_actor_count> old_transient_actors{};
    int32 old_transient_actor_count{0};
    int32 initial_actor_count{0};
    bool reset_complete{false};
};
}
