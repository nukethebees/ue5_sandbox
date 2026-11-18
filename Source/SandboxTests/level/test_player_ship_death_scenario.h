#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

namespace ioj {
class FTestPlayerShipDeathScenario final : public FSimulationTestScenario {
  public:
    explicit FTestPlayerShipDeathScenario(FSimulationTestContext& context);
    void run() override;
};
}
