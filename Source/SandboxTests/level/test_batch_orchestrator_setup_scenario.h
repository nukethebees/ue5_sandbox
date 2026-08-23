#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

namespace ml {
enum class EOrchestratorSetupScenario : uint8 { SpawnMissingActors, SimulationClockConversions };

class FTestBatchOrchestratorSetupScenario final : public FSimulationTestScenario {
  public:
    FTestBatchOrchestratorSetupScenario(FSimulationTestContext& context,
                                        EOrchestratorSetupScenario scenario);
    void run() override;
  private:
    void spawn_missing_actors();
    void simulation_clock_conversions();

    EOrchestratorSetupScenario scenario_;
};
}
