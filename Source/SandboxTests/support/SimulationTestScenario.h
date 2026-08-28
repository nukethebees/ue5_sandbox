#pragma once

#include "SoftTestAssertions.h"
#include "TestSimulationDriver.h"

#include <CoreMinimal.h>
#include <CQTest.h>
#include <Misc/Optional.h>
#include <Templates/UniquePtr.h>

class ATestBatchOrchestrator;
class FAutomationTestBase;
class FTestCommandBuilder;
class USpaceGameLevelConfig;
class UWorld;

namespace ml {
struct FSimulationTestContext {
    ATestBatchOrchestrator& orchestrator;
    FAutomationTestBase& automation_test;
    FTestCommandBuilder& command_builder;
    FSoftTestAssertions& checks;
    USpaceGameLevelConfig const& config;
    UWorld& world;
    int32 level_construction_count;
};

class FSimulationTestScenario {
  public:
    explicit FSimulationTestScenario(FSimulationTestContext& context)
        : context_{context}
        , TestRunner{&context.automation_test}
        , TestCommandBuilder{context.command_builder}
        , checks{context.checks}
        , Assert{context.automation_test} {}
    virtual ~FSimulationTestScenario() = default;

    virtual void run() = 0;
    void tear_down();
  protected:
    using time_type = TestSimulationDriver::time_type;

    auto initialise_test_driver() -> TestSimulationDriver&;

    template <typename Setup, typename Completion>
    void run_until_timeline_finished(Setup&& setup,
                                     FTimespan const timeout,
                                     Completion&& completion) {
        TestCommandBuilder.Do(Forward<Setup>(setup))
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then(Forward<Completion>(completion));
    }

    virtual void on_tear_down() {}

    FSimulationTestContext& context_;
    FAutomationTestBase* TestRunner;
    FTestCommandBuilder& TestCommandBuilder;
    FSoftTestAssertions& checks;
    FNoDiscardAsserter Assert;
    TOptional<TestSimulationDriver> test_driver{NullOpt};
};
}
