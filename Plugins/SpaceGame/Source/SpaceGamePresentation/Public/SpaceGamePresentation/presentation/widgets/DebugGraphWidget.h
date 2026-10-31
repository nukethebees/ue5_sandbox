#pragma once

#include <ioj/vector.h>

#include "SandboxUI/widgets/SGraphPlot.h"

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include <cstdint>
#include <span>

#include "DebugGraphWidget.generated.h"

class SGraphPlot;
namespace ioj {
struct FGameHudStyle;
}

UCLASS()
class SPACEGAMEPRESENTATION_API UDebugGraphWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_samples(std::span<ioj::Vector2d const> samples, std::uint32_t oldest_index);
    void apply_hud_style(ioj::FGameHudStyle const& style);
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
