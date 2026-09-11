#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UMaterialInstanceDynamic;
class SImage;

class SANDBOXUI_API SEntityOverlayWidget final : public SCompoundWidget {
  public:
    inline static constexpr TCHAR const* glow_material_path{
        TEXT("/SandboxUI/Generated/Materials/M_UiGlowComposite.M_UiGlowComposite")};
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
    TStrongObjectPtr<UTextureRenderTarget2D> glow_texture_;
    TStrongObjectPtr<UMaterialInstanceDynamic> glow_material_;
    FSlateBrush brush_;
    FSlateBrush glow_brush_;
    TSharedPtr<SImage> core_image_;
    TSharedPtr<SImage> glow_image_;
};
