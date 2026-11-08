#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <UObject/StrongObjectPtr.h>

#include <optional>

namespace ioj {
class USpaceGameLevelConfig;

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
    void check_terminal_completion();
    void on_tear_down() override;

    std::optional<FModalSample> modal_sample_;
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
    inline static FTimespan const timeout{0, 0, 2};
  public:
    explicit FLevelLoaderScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void load_fixture();
    void sample_runtime(ATestBatchOrchestrator& orchestrator);
    void sample_controller_lifecycle();
    void check_runtime();

    std::optional<FControlLifecycleSample> control_sample_;
};
}
