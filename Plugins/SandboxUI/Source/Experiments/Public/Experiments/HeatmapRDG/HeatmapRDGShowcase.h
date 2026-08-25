#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"
#include "UObject/ObjectPtr.h"

#include "HeatmapRDGShowcase.generated.h"

class UHeatmapRDGWidget;

UCLASS(Blueprintable)
class EXPERIMENTS_API UHeatmapRDGShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()
  private:
    enum class EPattern : uint8 { Hotspots, GradientChecker };
  public:
    UHeatmapRDGShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto select_grid_size(int32 grid_size) -> FReply;
    auto show_hotspots() -> FReply;
    auto show_gradient() -> FReply;
    void regenerate_selected_pattern();
    void generate_gradient_grid();

    UPROPERTY(Transient)
    TObjectPtr<UHeatmapRDGWidget> heatmap_widget_;

    int32 grid_size_{128};
    EPattern selected_pattern_{EPattern::Hotspots};
};
