#pragma once

#include <ioj/sim/levels/level_state.h>

#include <CommonActivatableWidget.h>
#include <TimerManager.h>

#include "BenchmarkHudWidget.generated.h"

class UBorder;
class UOverlay;
class UTextBlock;
class UValueWidget;

namespace ioj {
class FGameUiStyle;
class UMenuButtonWidget;
}

struct FRunHudState {
    TOptional<uint64> ticks_remaining;
    ioj::sim::LevelState state{ioj::sim::LevelState::Uninitialised};
};

UCLASS()
class SPACEGAMEPRESENTATION_API UBenchmarkHudWidget : public UCommonActivatableWidget {
    GENERATED_BODY()
  public:
    void apply_ui_style(ioj::FGameUiStyle const& style);
    void set_state_source(TFunction<FRunHudState()> source);

    TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    FSimpleMulticastDelegate end_requested;
    FSimpleMulticastDelegate pause_requested;
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
    ioj::UMenuButtonWidget* pause_resume_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    ioj::UMenuButtonWidget* end_benchmark_button{nullptr};
  private:
    void update_state();
    void handle_end_requested();
    void handle_pause_requested();
    void clear_update_timer();

    TFunction<FRunHudState()> state_source_;
    FTimerHandle update_timer_{};
};
