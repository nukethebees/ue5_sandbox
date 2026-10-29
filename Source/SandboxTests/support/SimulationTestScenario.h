#pragma once

#include "simulation_step.h"
#include "SoftTestAssertions.h"
#include "TestSimulationDriver.h"
#include "time_series_test_data.h"

#include <CoreMinimal.h>
#include <CQTest.h>
#include <Misc/Optional.h>
#include <Templates/UniquePtr.h>

class ATestBatchOrchestrator;
class FAutomationTestBase;
class FTestCommandBuilder;
class USpaceGameLevelConfig;
class UWorld;

namespace ioj {
DECLARE_DELEGATE_OneParam(FSimulationSampleCallback, ATestBatchOrchestrator&);

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
        TestCommandBuilder
            .Do([this, setup = Forward<Setup>(setup)]() mutable {
                setup();
                context_.orchestrator.SetActorTickEnabled(false);
            })
            .Until([this] { return advance_until_timeline_finished(); }, timeout)
            .Then(Forward<Completion>(completion));
    }

    void advance_sample();
    auto advance_until_timeline_finished() -> bool;
    void set_sample_callback(FSimulationSampleCallback callback) {
        sample_callback_ = MoveTemp(callback);
    }

    template <typename... ValueTypes>
    void begin_timed_sampling(time_type const duration,
                              FSimulationSampleCallback sample_hook,
                              TimeSeriesData<ValueTypes>&... samples) {
        check(test_driver.IsSet());

        reset_and_reserve_time_series(test_driver->orchestrator, duration, samples...);
        test_driver->timeline.finish_at(duration);
        set_sample_callback(MoveTemp(sample_hook));
    }

    virtual void on_tear_down() {}

    FSimulationTestContext& context_;
    FAutomationTestBase* TestRunner;
    FTestCommandBuilder& TestCommandBuilder;
    FSoftTestAssertions& checks;
    FNoDiscardAsserter Assert;
    TOptional<TestSimulationDriver> test_driver{NullOpt};
    FSimulationSampleCallback sample_callback_;
};
}
