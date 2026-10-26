#pragma once

#include "Components/Widget.h"
#include "Containers/Array.h"
#include "Math/IntPoint.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

#include "HeatmapRDGWidget.generated.h"

class SImage;
class UTextureRenderTarget2D;

namespace SlateGenerated {
struct UHeatmapRDGWidgetBuilder;
}

USTRUCT(BlueprintType)
struct SBXUIEXPERIMENTS_API FHeatmapRDGGrid {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    TArray<float> values;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    int32 width{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heatmap")
    int32 height{0};

    [[nodiscard]] auto is_valid() const noexcept -> bool;
};

UCLASS(meta = (DisplayName = "RDG Heatmap (Experiment)"))
class SBXUIEXPERIMENTS_API UHeatmapRDGWidget : public UWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::UHeatmapRDGWidgetBuilder;
  public:
    UHeatmapRDGWidget();

    UFUNCTION(BlueprintCallable, Category = "Heatmap|Experiment")
    [[nodiscard]] bool set_grid(FHeatmapRDGGrid const& grid);

    UFUNCTION(BlueprintCallable, Category = "Heatmap|Experiment")
    void generate_demo_grid(int32 width = 128, int32 height = 128);
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;

#if WITH_EDITOR
    const FText GetPaletteCategory() override;
#endif
  private:
    [[nodiscard]] auto ensure_output_texture(FIntPoint dimensions) -> bool;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> output_texture_;

    FSlateBrush brush_;
    TSharedPtr<SImage> image_;
    FIntPoint output_size_{FIntPoint::ZeroValue};
    bool has_submitted_grid_{false};
};
