#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

class AActor;

namespace ioj {
class FTestBatchOrchestratorResetScenario final : public FSimulationTestScenario {

    static constexpr int32 blocker_count{3};
    static constexpr int32 max_transient_actor_count{64};

  public:
    explicit FTestBatchOrchestratorResetScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_blockers(UWorld& world);
    auto count_actors(UWorld const& world) const -> int32;
    auto is_core_runtime_actor(AActor const& actor) const -> bool;
    void save_old_transient_actors(ATestBatchOrchestrator const& orchestrator);
    void start_initial_simulation();
    void reset_simulation();
    void check_reset();

    TStaticArray<TWeakObjectPtr<AActor>, blocker_count> blockers{};
    ATestSpaceShip const* old_player{};
    TStaticArray<AActor*, max_transient_actor_count> old_transient_actors{};
    int32 old_transient_actor_count{0};
    int32 initial_actor_count{0};
    int32 initial_ledger_alive{0};
};
}
