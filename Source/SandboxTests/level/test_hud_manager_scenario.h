#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

namespace ioj {
class FTestHUDManagerScenario final : public FSimulationTestScenario {
  public:
    explicit FTestHUDManagerScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void registration_process_samples();
};
}
