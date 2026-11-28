#pragma once

#include <ioj/input/presentation_events.h>
#include <ioj/presentation/level_frame.h>

#include <Blueprint/UserWidget.h>

#include "BenchmarkHudWidget.generated.h"

class UBorder;
class UOverlay;
class UTextBlock;

namespace ioj {
class UValueWidget;
class FGameUiStyle;
class UMenuButtonWidget;

UCLASS()
class SPACEGAMEPRESENTATION_API UBenchmarkHudWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void apply_ui_style(FGameUiStyle const& style);
    void present_status(presentation::LevelStatus const& status);
    void select_button(host::BenchmarkAction action);

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
    UMenuButtonWidget* pause_resume_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* end_benchmark_button{nullptr};
  private:
    void handle_end_requested();
    void handle_pause_requested();
};

} // namespace ioj
