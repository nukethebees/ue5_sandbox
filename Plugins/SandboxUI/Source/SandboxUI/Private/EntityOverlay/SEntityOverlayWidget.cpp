#include "SandboxUI/EntityOverlay/SEntityOverlayWidget.h"

#include "EntityOverlayRenderer.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RHIGlobals.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"

DEFINE_LOG_CATEGORY_STATIC(LogEntityOverlayWidget, Log, All);

SEntityOverlayWidget::SEntityOverlayWidget() = default;

void SEntityOverlayWidget::Construct(FArguments const&) {
    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;

    glow_brush_ = brush_;
    if (auto* const material{LoadObject<UMaterialInterface>(nullptr, glow_material_path)}) {
        glow_material_.Reset(UMaterialInstanceDynamic::Create(material, GetTransientPackage()));
        glow_brush_.SetResourceObject(glow_material_.Get());
        set_style(style_);
    } else {
        UE_LOG(LogEntityOverlayWidget,
               Error,
               TEXT("Missing UI glow composite material: %s"),
               glow_material_path);
    }
    ChildSlot[SNew(SOverlay) +
              SOverlay::Slot()[SAssignNew(glow_image_, SImage)
                                   .Image(&glow_brush_)
                                   .Visibility(EVisibility::Collapsed)] +
              SOverlay::Slot()[SAssignNew(core_image_, SImage).Image(&brush_)]];
}

SEntityOverlayWidget::~SEntityOverlayWidget() {
    brush_.SetResourceObject(nullptr);
    glow_brush_.SetResourceObject(nullptr);
    glow_material_.Reset();
    glow_texture_.Reset();
    output_texture_.Reset();
}

void SEntityOverlayWidget::set_frame_store(FEntityOverlayFrameStoreConstPtr frame_store) {
    frame_store_ = MoveTemp(frame_store);
}

void SEntityOverlayWidget::set_style(FEntityOverlayStyle const& style) {
    style_ = style;
    if (glow_material_.IsValid()) {
        glow_material_->SetVectorParameterValue(TEXT("GlowColor"),
                                                style.soft_target_in_range_color);
        glow_material_->SetScalarParameterValue(
            TEXT("PreserveCorePixels"), style.soft_target_glow.preserve_core_pixels ? 1.0f : 0.0f);
    }
}

#if WITH_EDITOR
void SEntityOverlayWidget::set_core_visibility(bool const visible) {
    core_image_->SetVisibility(visible ? EVisibility::HitTestInvisible : EVisibility::Hidden);
}
#endif

void SEntityOverlayWidget::render(FEntityOverlayView const& view) {
    if (GUsingNullRHI || !frame_store_.IsValid() || !view.is_valid() ||
        !ensure_output_texture(view.output_size)) {
        glow_image_->SetVisibility(EVisibility::Collapsed);
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
    auto* const glow_resource{
        glow_texture_.IsValid() ? glow_texture_->GameThread_GetRenderTargetResource() : nullptr};
    auto const& frame{frame_store_->current()};
    bool const show_glow{
        glow_resource != nullptr && !frame.instances.IsEmpty() &&
        style_.soft_target_glow.intensity > 0.0f &&
        ((frame.soft_target_in_range && frame.soft_target_visibility > 0.0f) ||
         (frame.fading_soft_target_in_range && frame.fading_soft_target_visibility > 0.0f))};
    glow_image_->SetVisibility(show_glow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    FEntityOverlayRenderer{}.render(frame_store_, view, style_, output_resource, glow_resource);
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
    if (glow_material_.IsValid()) {
        auto* const glow_texture{NewObject<UTextureRenderTarget2D>()};
        glow_texture->ClearColor = FLinearColor::Transparent;
        glow_texture->Filter = TF_Bilinear;
        glow_texture->AddressX = TA_Clamp;
        glow_texture->AddressY = TA_Clamp;
        glow_texture->InitCustomFormat(output_size.X, output_size.Y, PF_R16F, true);
        glow_texture_.Reset(glow_texture);
        glow_brush_.ImageSize = FVector2D{output_size};
        glow_material_->SetTextureParameterValue(TEXT("GlowEnergy"), glow_texture);
        glow_material_->SetTextureParameterValue(TEXT("CoreTexture"), output_texture);
        glow_material_->SetVectorParameterValue(TEXT("GlowColor"),
                                                style_.soft_target_in_range_color);
    }
    return true;
}
