#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>

namespace ml {
class FLevelLoaderCameraScenario final : public FSimulationTestScenario {
    inline static FTimespan const timeout{0, 0, 2};
  public:
    explicit FLevelLoaderCameraScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void load_fixture();
    void sample_runtime(ATestBatchOrchestrator& orchestrator);
    void check_runtime();
    void on_tear_down() override;

    TimeSeriesData<int32> entity_counts_;
    TWeakObjectPtr<AActor> camera_;
};

class FLevelLoaderScenario final : public FSimulationTestScenario {
    struct FSample {
        int32 authored_entities{0};
        int32 blue_players{0};
        int32 blue_capitals{0};
        int32 red_capitals{0};
        int32 red_turrets{0};
        ETestMissionMode mission_mode{ETestMissionMode::None};
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        int32 mission_kill_target{0};
        int32 mission_heroes{0};
        int32 mission_survivors{0};
        int32 mission_required_kills{0};
        FName mission_level_name{NAME_None};
        bool saves_mission_results{true};
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
