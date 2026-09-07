#include "SandboxUI/Radar/SRadarWidget.h"

#include "RadarRenderer.h"

#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadarWidget, Log, All);

void SRadarWidget::Construct(FArguments const&) {
    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
    brush_.ImageSize = FVector2D{output_texture_dimension, output_texture_dimension};
    ChildSlot[SNew(SImage).Image(&brush_)];
}

SRadarWidget::~SRadarWidget() {
    brush_.SetResourceObject(nullptr);
    output_texture_.Reset();
}

void SRadarWidget::set_frame_store(FRadarFrameStoreConstPtr frame_store) {
    frame_store_ = MoveTemp(frame_store);
}

void SRadarWidget::set_style(FRadarStyle const& style) {
    style_ = style;
}

void SRadarWidget::render() {
    if (GetVisibility() == EVisibility::Collapsed || GetVisibility() == EVisibility::Hidden ||
        GUsingNullRHI || !frame_store_.IsValid() || !ensure_output_texture()) {
        return;
    }

    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogRadarWidget, Error, TEXT("Failed to acquire the radar render-target resource."));
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::Submit);
    FRadarRenderer{}.render(frame_store_, style_, output_resource);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SRadarWidget::ensure_output_texture() -> bool {
    if (output_texture_.IsValid()) {
        return true;
    }

    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogRadarWidget, Error, TEXT("Failed to allocate the radar render target."));
        return false;
    }

    output_texture->ClearColor = FLinearColor::Transparent;
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(
        output_texture_dimension, output_texture_dimension, PF_R8G8B8A8, true);
    output_texture_.Reset(output_texture);
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}
