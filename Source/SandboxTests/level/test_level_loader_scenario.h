#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <UObject/StrongObjectPtr.h>

namespace ioj {
class USpaceGameLevelConfig;

enum class ELevelLoaderScenario : uint8 {
    PlayerMaterialisation,
    PossessionDuringPause,
    ObserverMaterialisation,
    ObserverModalTransitions,
    ObserverBenchmarkSwitching,
    HeadlessRunControls,
    TerminalCompletion,
};

class FLevelLoaderScenario final : public FSimulationTestScenario {
  public:
    FLevelLoaderScenario(FSimulationTestContext& context, ELevelLoaderScenario scenario);
    void run() override;
  private:
    void start_simulation();
    void load_player_fixture();
    void check_player_materialisation();
    void check_possession_during_pause();
    void load_observer_fixture();
    void check_observer_materialisation();
    void check_observer_modal_transitions();
    void activate_observer_benchmark();
    void check_observer_benchmark_switching();
    void check_headless_run_controls();
    void check_terminal_completion();
    void on_tear_down() override;

    ELevelLoaderScenario scenario_;
    FTransform observer_transform_;
    TWeakObjectPtr<AActor> camera_;
    TStrongObjectPtr<USpaceGameLevelConfig> original_config_;
};
}
