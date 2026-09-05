#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SANDBOXUI_API SEntityOverlayWidget final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SEntityOverlayWidget) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);
    ~SEntityOverlayWidget() override;

    void set_frame_store(FEntityOverlayFrameStoreConstPtr frame_store);
    void set_style(FEntityOverlayStyle const& style);
    void render(FEntityOverlayView const& view);
  private:
    [[nodiscard]] auto ensure_output_texture(FIntPoint output_size) -> bool;

    FEntityOverlayFrameStoreConstPtr frame_store_;
    FEntityOverlayStyle style_;
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
};
