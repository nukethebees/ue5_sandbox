#pragma once

#include "Scatter3DRenderer.h"

#include "Containers/Array.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class SScatter3DWidget final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SScatter3DWidget) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    ~SScatter3DWidget() override;

    void set_point_count(int32 point_count);
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
  private:
    void initialise_points(int32 point_count);
    [[nodiscard]] auto initialise_output_texture() -> bool;
    void submit_render();

    FScatter3DRenderer renderer_;
    TArray<FScatter3DPoint> points_;
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
    TSharedPtr<SImage> image_;
    int32 initial_render_delay_ticks_{2};
    bool output_ready_{false};
};
