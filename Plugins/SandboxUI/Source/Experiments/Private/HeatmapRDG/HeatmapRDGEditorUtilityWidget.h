#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "UObject/ObjectPtr.h"

#include "HeatmapRDGEditorUtilityWidget.generated.h"

class UHeatmapRDGWidget;

UCLASS()
class UHeatmapRDGEditorUtilityWidget : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    UHeatmapRDGEditorUtilityWidget();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto select_grid_size(int32 grid_size) -> FReply;
    auto show_hotspots() -> FReply;
    auto show_gradient() -> FReply;
    void generate_gradient_grid();

    UPROPERTY(Transient)
    TObjectPtr<UHeatmapRDGWidget> heatmap_widget_;

    int32 grid_size_{128};
};
