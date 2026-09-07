#pragma once

#include <CommonActivatableWidget.h>
#include <TimerManager.h>

#include "BenchmarkHudWidget.generated.h"

class ATestBatchOrchestrator;
class UBorder;
class UOverlay;
class UTextBlock;
class UValueWidget;

namespace ml::ioj {
class FGameUiStyle;
class UMenuButtonWidget;
}

UCLASS()
class SPACEGAME_API UBenchmarkHudWidget : public UCommonActivatableWidget {
    GENERATED_BODY()
  public:
    void apply_ui_style(ml::ioj::FGameUiStyle const& style);
    void set_orchestrator(ATestBatchOrchestrator& orchestrator);

    TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    FSimpleMulticastDelegate end_requested;
  protected:
    void NativeOnInitialized() override;
    void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* benchmark_root{nullptr};

    UPROPERTY(meta = (BindWidget))
    UBorder* benchmark_panel{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* benchmark_title{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* ticks_remaining_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    ml::ioj::UMenuButtonWidget* end_benchmark_button{nullptr};
  private:
    void update_ticks_remaining();
    void handle_end_requested();
    void clear_update_timer();

    TWeakObjectPtr<ATestBatchOrchestrator> orchestrator_{};
    FTimerHandle update_timer_{};
};
