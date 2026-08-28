#pragma once

#include "VolumeHeatmap3DRenderer.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SImage;

enum class EVolumeHeatmap3DPattern : uint8 {
    GaussianClouds,
    HollowShell,
};

class SVolumeHeatmap3DWidget final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SVolumeHeatmap3DWidget) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    ~SVolumeHeatmap3DWidget() override;

    void set_pattern(EVolumeHeatmap3DPattern pattern);
    void set_grid_dimension(int32 dimension);
    void set_slice_count(int32 slice_count);
    void set_yaw(float yaw_degrees);
    void set_pitch(float pitch_degrees);
    void set_density_scale(float density_scale);
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
  private:
    void initialise_grid();
    [[nodiscard]] auto initialise_output_texture() -> bool;
    void request_render();
    void submit_render();

    FVolumeHeatmap3DRenderer renderer_;
    FVolumeHeatmap3DGrid grid_;
    FVolumeHeatmap3DView view_;
    EVolumeHeatmap3DPattern pattern_{EVolumeHeatmap3DPattern::GaussianClouds};
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
    TSharedPtr<SImage> image_;
    int32 initial_render_delay_ticks_{2};
    bool output_ready_{false};
};
