#pragma once

#include <Components/SlateWrapperTypes.h>
#include <CoreMinimal.h>
#include <SpaceGame/ships/player/PlayerControlContext.h>
#include "PlayerHudLifecycle.generated.h"

class ASpaceGamePlayerController;
class ATestBatchOrchestrator;
class USimulationHudWidget;
class UBenchmarkHudWidget;
class UTestBatchGameUiData;

USTRUCT()
struct SPACEGAME_API FPlayerHudLifecycle {
    GENERATED_BODY()

    auto initialise(ASpaceGamePlayerController& owner,
                    ATestBatchOrchestrator* orchestrator,
                    UTestBatchGameUiData* ui_data,
                    EPlayerControlContext context,
                    float observer_speed) -> bool;
    void shutdown();
    auto initialise_benchmark(ASpaceGamePlayerController& owner,
                              ATestBatchOrchestrator* orchestrator,
                              UTestBatchGameUiData* ui_data) -> bool;
    void shutdown_benchmark(ASpaceGamePlayerController& owner);
    void hide_for_modal();
    void restore_after_modal();
    void cancel_restore();
    void set_observer_speed(float speed);
    auto get_hud() const -> USimulationHudWidget const* { return hud_widget_; }
    auto get_benchmark_hud() const -> UBenchmarkHudWidget const* { return benchmark_hud_widget_; }
  private:
    TWeakObjectPtr<ATestBatchOrchestrator> registered_orchestrator_;
    UPROPERTY(Transient)
    TObjectPtr<USimulationHudWidget> hud_widget_{nullptr};
    UPROPERTY(Transient)
    TObjectPtr<UBenchmarkHudWidget> benchmark_hud_widget_{nullptr};
    bool restore_pending_{false};
    ESlateVisibility visibility_before_modal_{ESlateVisibility::Visible};
};
