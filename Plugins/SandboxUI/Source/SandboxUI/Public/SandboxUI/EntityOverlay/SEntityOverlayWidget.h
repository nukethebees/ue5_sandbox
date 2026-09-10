#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SImage;

class SANDBOXUI_API SEntityOverlayWidget final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SEntityOverlayWidget) {}
    SLATE_END_ARGS()

    SEntityOverlayWidget();
    void Construct(FArguments const& arguments);
    ~SEntityOverlayWidget() override;

    void set_frame_store(FEntityOverlayFrameStoreConstPtr frame_store);
    void set_style(FEntityOverlayStyle const& style);
    void render(FEntityOverlayView const& view);
#if WITH_EDITOR
    void set_core_visibility(bool visible);
#endif
  private:
    [[nodiscard]] auto ensure_output_texture(FIntPoint output_size) -> bool;

    FEntityOverlayFrameStoreConstPtr frame_store_;
    FEntityOverlayStyle style_;
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
    TSharedPtr<SImage> core_image_;
};
