#pragma once

#include <ioj/levels/control_context.h>

#include <Components/SlateWrapperTypes.h>
#include <CoreMinimal.h>

#include "PlayerHudLifecycle.generated.h"

namespace ioj {
class USimulationHudWidget;
class UBenchmarkHudWidget;
class UTestBatchGameUiData;
class ASpaceGamePlayerController;
class ATestBatchOrchestrator;

USTRUCT()
struct SPACEGAME_API FPlayerHudLifecycle {
    GENERATED_BODY()

    auto initialise(ASpaceGamePlayerController& owner,
                    ATestBatchOrchestrator* orchestrator,
                    UTestBatchGameUiData* ui_data,
                    levels::ControlContext context,
                    float observer_speed) -> bool;
    void shutdown();
    auto initialise_benchmark(ASpaceGamePlayerController& owner,
                              ATestBatchOrchestrator* orchestrator,
                              UTestBatchGameUiData* ui_data) -> bool;
    void shutdown_benchmark();
    void update_run_controls();
    void hide_for_modal();
    void restore_after_modal();
    void set_observer_speed(float speed);
    void set_mouse_steering(TOptional<FVector2D> displacement);
    auto get_hud() const -> USimulationHudWidget const* { return hud_widget_; }
    auto get_benchmark_hud() const -> UBenchmarkHudWidget* { return benchmark_hud_widget_; }
  private:
    TWeakObjectPtr<ATestBatchOrchestrator> registered_orchestrator_;
    UPROPERTY(Transient)
    TObjectPtr<USimulationHudWidget> hud_widget_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<UBenchmarkHudWidget> benchmark_hud_widget_{nullptr};
    TOptional<ESlateVisibility> visibility_before_modal_;
};

} // namespace ioj
