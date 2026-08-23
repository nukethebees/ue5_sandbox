#pragma once

#include "SoftTestAssertions.h"

#include <CQTest.h>
#include <CoreMinimal.h>
#include <Templates/UniquePtr.h>

class ATestBatchOrchestrator;
class FAutomationTestBase;
class FTestCommandBuilder;
class UTestSimulationConfig;
class UWorld;

namespace ml {
struct FSimulationTestContext {
    ATestBatchOrchestrator& orchestrator;
    FAutomationTestBase& automation_test;
    FTestCommandBuilder& command_builder;
    FSoftTestAssertions& checks;
    UTestSimulationConfig const& config;
    UWorld& world;
    int32 level_construction_count;
};

class FSimulationTestScenario {
  public:
    explicit FSimulationTestScenario(FSimulationTestContext& context)
        : context_{context},
          TestRunner{&context.automation_test},
          TestCommandBuilder{context.command_builder},
          checks{context.checks},
          Assert{context.automation_test} {}
    virtual ~FSimulationTestScenario() = default;

    virtual void run() = 0;
    virtual void tear_down() {}
  protected:
    FSimulationTestContext& context_;
    FAutomationTestBase* TestRunner;
    FTestCommandBuilder& TestCommandBuilder;
    FSoftTestAssertions& checks;
    FNoDiscardAsserter Assert;
};
}
