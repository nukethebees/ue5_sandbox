#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
class FLevelLoaderScenario final : public FSimulationTestScenario {
    struct FSample {
        int32 authored_entities{0};
        int32 blue_players{0};
        int32 blue_capitals{0};
        int32 red_capitals{0};
        int32 red_turrets{0};
        FVector3f blue_capital_position{FVector3f::ZeroVector};
        FVector3f red_capital_position{FVector3f::ZeroVector};
        FVector3f red_turret_position{FVector3f::ZeroVector};
    };

    inline static FTimespan const timeout{0, 0, 2};
  public:
    explicit FLevelLoaderScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void load_fixture();
    void sample_runtime(ATestBatchOrchestrator& orchestrator);
    void check_runtime();

    TimeSeriesData<FSample> samples;
};
}
