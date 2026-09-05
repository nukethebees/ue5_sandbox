#include "SandboxUI/EntityOverlay/SEntityOverlayWidget.h"

#include "EntityOverlayRenderer.h"

#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogEntityOverlayWidget, Log, All);

void SEntityOverlayWidget::Construct(FArguments const&) {
    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;

    ChildSlot[SNew(SImage).Image(&brush_)];
}

SEntityOverlayWidget::~SEntityOverlayWidget() {
    brush_.SetResourceObject(nullptr);
    output_texture_.Reset();
}

void SEntityOverlayWidget::set_frame(FEntityOverlayFramePtr frame) {
    frame_ = MoveTemp(frame);
}

void SEntityOverlayWidget::set_style(FEntityOverlayStyle const& style) {
    style_ = style;
}

void SEntityOverlayWidget::render(FEntityOverlayView const& view) {
    if (GUsingNullRHI || !frame_.IsValid() || !view.is_valid() ||
        !ensure_output_texture(view.output_size)) {
        return;
    }

    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogEntityOverlayWidget,
               Error,
               TEXT("Failed to acquire the entity overlay render-target resource."));
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::Submit);
    FEntityOverlayRenderer{}.render(frame_, view, style_, output_resource);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SEntityOverlayWidget::ensure_output_texture(FIntPoint const output_size) -> bool {
    if (output_texture_.IsValid() && output_texture_->SizeX == output_size.X &&
        output_texture_->SizeY == output_size.Y) {
        return true;
    }

    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogEntityOverlayWidget,
               Error,
               TEXT("Failed to allocate the entity overlay render target."));
        return false;
    }

    output_texture->ClearColor = FLinearColor::Transparent;
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(output_size.X, output_size.Y, PF_R8G8B8A8, true);
    output_texture_.Reset(output_texture);
    brush_.ImageSize = FVector2D{output_size};
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}
