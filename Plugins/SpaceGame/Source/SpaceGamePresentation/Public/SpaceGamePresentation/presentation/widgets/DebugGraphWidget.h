#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SandboxUI/widgets/SGraphPlot.h"

#include "DebugGraphWidget.generated.h"

class SGraphPlot;
namespace ml::ioj {
struct FGameHudStyle;
}

UCLASS()
class SPACEGAMEPRESENTATION_API UDebugGraphWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_samples(TConstArrayView<FVector2d> in_samples, int32 oldest_index);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    void update_slate_series();

    TArray<float> x_;
    TArray<float> y_;
    TSharedPtr<SGraphPlot> graph_widget_;
    TOptional<FGraphPlotStyle> graph_style_{};
    FLinearColor series_colour_{FLinearColor::Green};
};
