#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

namespace ioj {
enum class EOrchestratorSetupScenario : uint8 {
    SpawnMissingActors,
    PresentationFrameOrdering,
};

class FTestBatchOrchestratorSetupScenario final : public FSimulationTestScenario {
  public:
    FTestBatchOrchestratorSetupScenario(FSimulationTestContext& context,
                                        EOrchestratorSetupScenario scenario);
    void run() override;
  private:
    void prepare_level();
    void presentation_frame_ordering();

    EOrchestratorSetupScenario scenario_;
};
}
