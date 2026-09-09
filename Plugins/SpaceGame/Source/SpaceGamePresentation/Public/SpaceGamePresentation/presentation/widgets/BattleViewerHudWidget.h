#pragma once

#include <SpaceGamePresentation/presentation/widgets/SimulationHudWidget.h>

#include "BattleViewerHudWidget.generated.h"

class UTextBlock;
class UValueWidget;

UCLASS()
class SPACEGAMEPRESENTATION_API UBattleViewerHudWidget : public USimulationHudWidget {
    GENERATED_BODY()
  public:
    void apply_ui_style(ml::ioj::FGameUiStyle const& style) override;
    void set_movement_speed(float speed);
  protected:
    void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* controls_label{nullptr};

    UPROPERTY(meta = (BindWidget))
    UValueWidget* camera_speed_widget{nullptr};
};
