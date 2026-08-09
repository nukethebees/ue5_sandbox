#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DebugGraphWidget.generated.h"

UCLASS()
class SANDBOX_API UDebugGraphWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_samples(TConstArrayView<FVector2d> in_samples, int32 oldest_index);

    int32 NativePaint(FPaintArgs const& args,
                      FGeometry const& geometry,
                      FSlateRect const& culling_rect,
                      FSlateWindowElementList& out_draw_elements,
                      int32 layer_id,
                      FWidgetStyle const& widget_style,
                      bool parent_enabled) const override;
  private:
    TArray<FVector2d> samples;
    int32 oldest_index{0};
    double max_value{0.0};
};
