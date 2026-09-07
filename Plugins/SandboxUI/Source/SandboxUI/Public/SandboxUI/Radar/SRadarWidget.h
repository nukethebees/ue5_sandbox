#pragma once

#include "SandboxUI/Radar/RadarFrameStore.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SANDBOXUI_API SRadarWidget final : public SCompoundWidget {
  public:
    static constexpr int32 output_texture_dimension{1024};

    SLATE_BEGIN_ARGS(SRadarWidget) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);
    ~SRadarWidget() override;

    void set_frame_store(FRadarFrameStoreConstPtr frame_store);
    void set_style(FRadarStyle const& style);
    void render();
  private:
    [[nodiscard]] auto ensure_output_texture() -> bool;

    FRadarFrameStoreConstPtr frame_store_;
    FRadarStyle style_;
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
};
