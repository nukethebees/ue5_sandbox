#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>
#include <UObject/StrongObjectPtr.h>

class USpaceGameLevelConfig;

namespace ml {
class FLevelLoaderCameraScenario final : public FSimulationTestScenario {
    struct FModalSample {
        bool pause_open{false};
        bool pause_suspended{false};
        bool pause_rejects_activation{false};
        bool hud_hidden{false};
        bool pause_resumed{false};
        bool completion_open{false};
        bool completion_suspended{false};
        bool completion_rejects_activation{false};
        bool completion_resumed{false};
        bool hud_restored{false};
        bool input_restored{false};
    };
    inline static FTimespan const timeout{0, 0, 2};
  public:
    explicit FLevelLoaderCameraScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void load_fixture();
    void load_headless_fixture();
    void sample_runtime(ATestBatchOrchestrator& orchestrator);
    void sample_modal_transitions();
    void check_runtime();
    void on_tear_down() override;

    TimeSeriesData<int32> entity_counts_;
    TimeSeriesData<FModalSample> modal_samples_;
    TWeakObjectPtr<AActor> camera_;
    TStrongObjectPtr<USpaceGameLevelConfig> original_config_;
};

class FLevelLoaderScenario final : public FSimulationTestScenario {
    struct FControlLifecycleSample {
        bool input_activated_after_initialisation{false};
        bool hud_created_after_initialisation{false};
        bool unpossessed_while_paused{false};
        bool resumed_without_ship{false};
        bool possession_enabled_ship{false};
        bool possession_stayed_suspended{false};
        bool resumed_with_ship{false};
        bool bindings_restored{false};
    };
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
        FString mission_level_display_name{};
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
    void sample_controller_lifecycle();
    void check_runtime();

    TimeSeriesData<FSample> samples;
    TimeSeriesData<FControlLifecycleSample> control_samples_;
};
}
