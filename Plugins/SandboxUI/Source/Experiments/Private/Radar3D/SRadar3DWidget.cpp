#include "SRadar3DWidget.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadar3DWidget, Log, All);

namespace {
constexpr int32 output_texture_dimension{512};
}

void SRadar3DWidget::Construct(FArguments const&) {
    initialise_contacts();

    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
    brush_.ImageSize = FVector2D{output_texture_dimension, output_texture_dimension};

    output_ready_ = initialise_output_texture();
    SetCanTick(true);

    ChildSlot[SAssignNew(image_, SImage).Image(&brush_)];

    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
    }
}

SRadar3DWidget::~SRadar3DWidget() {
    brush_.SetResourceObject(nullptr);
    image_.Reset();
    output_texture_.Reset();
}

void SRadar3DWidget::Tick(FGeometry const& allotted_geometry,
                          double const current_time,
                          float const delta_time) {
    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);

    elapsed_time_ += delta_time;
    auto& moving_contact{contacts_[0]};
    moving_contact.position = FVector3f{0.72f * FMath::Cos(elapsed_time_ * 0.8f),
                                        0.72f * FMath::Sin(elapsed_time_ * 0.8f),
                                        0.32f + 0.22f * FMath::Sin(elapsed_time_ * 1.3f)};

    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SRadar3DWidget::initialise_contacts() {
    contacts_ = {
        {.position = FVector3f{0.72f, 0.0f, 0.32f},
         .size = 7.0f,
         .color = FVector4f{1.0f, 0.28f, 0.12f, 1.0f}},
        {.position = FVector3f{-0.58f, -0.32f, 0.55f},
         .size = 6.0f,
         .color = FVector4f{0.2f, 0.85f, 1.0f, 1.0f}},
        {.position = FVector3f{0.22f, 0.62f, -0.42f},
         .size = 5.5f,
         .color = FVector4f{0.42f, 1.0f, 0.38f, 1.0f}},
        {.position = FVector3f{-0.18f, 0.08f, 0.68f},
         .size = 5.0f,
         .color = FVector4f{1.0f, 0.88f, 0.22f, 1.0f}},
        {.position = FVector3f{0.62f, -0.55f, -0.18f},
         .size = 5.5f,
         .color = FVector4f{0.75f, 0.38f, 1.0f, 1.0f}},
    };
}

auto SRadar3DWidget::initialise_output_texture() -> bool {
    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogRadar3DWidget, Error, TEXT("Failed to allocate the 3D radar render target."));
        return false;
    }

    output_texture->bSupportsUAV = true;
    output_texture->ClearColor = FLinearColor{0.003f, 0.008f, 0.012f, 1.0f};
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(
        output_texture_dimension, output_texture_dimension, PF_R8G8B8A8, true);

    output_texture_.Reset(output_texture);
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}

void SRadar3DWidget::submit_render() {
    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogRadar3DWidget,
               Error,
               TEXT("Failed to acquire the 3D radar render-target resource."));
        output_ready_ = false;
        return;
    }

    renderer_.render(contacts_, output_resource);
}
