#include "SScatter3DWidget.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

#include "generated/SScatter3DWidget.slate.generated.h"

DEFINE_LOG_CATEGORY_STATIC(LogScatter3DWidget, Log, All);

namespace {
constexpr int32 scatter_3d_output_texture_dimension{512};
constexpr int32 initial_point_count{4096};
}

void SScatter3DWidget::Construct(FArguments const&) {
    initialise_points(initial_point_count);

    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
    brush_.ImageSize =
        FVector2D{scatter_3d_output_texture_dimension, scatter_3d_output_texture_dimension};

    output_ready_ = initialise_output_texture();
    SetCanTick(true);

    ChildSlot[SlateGenerated::SScatter3DWidgetBuilder{*this}.BuildImage(&brush_)];
}

void SScatter3DWidget::set_point_count(int32 const point_count) {
    check(point_count > 0);
    initialise_points(point_count);

    if (output_ready_ && !GUsingNullRHI) {
        if (initial_render_delay_ticks_ == 0) {
            submit_render();
            Invalidate(EInvalidateWidgetReason::Paint);
        }
    }
}

SScatter3DWidget::~SScatter3DWidget() {
    brush_.SetResourceObject(nullptr);
    image_.Reset();
    output_texture_.Reset();
}

void SScatter3DWidget::Tick(FGeometry const& allotted_geometry,
                            double const current_time,
                            float const delta_time) {
    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);

    if (initial_render_delay_ticks_ == 0) {
        return;
    }
    if (--initial_render_delay_ticks_ > 0) {
        return;
    }

    initial_render_delay_ticks_ = 0;
    SetCanTick(false);
    if (output_ready_ && !GUsingNullRHI) {
        submit_render();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SScatter3DWidget::initialise_points(int32 const point_count) {
    struct FCluster {
        FVector3f centre;
        FVector3f extent;
        FVector4f color;
    };
    FCluster const clusters[]{
        {FVector3f{-0.46f, -0.30f, 0.22f},
         FVector3f{0.30f, 0.20f, 0.32f},
         FVector4f{0.12f, 0.72f, 1.0f, 0.92f}},
        {FVector3f{0.40f, -0.16f, -0.32f},
         FVector3f{0.24f, 0.34f, 0.20f},
         FVector4f{1.0f, 0.42f, 0.12f, 0.92f}},
        {FVector3f{0.18f, 0.48f, 0.30f},
         FVector3f{0.34f, 0.22f, 0.24f},
         FVector4f{0.48f, 1.0f, 0.30f, 0.92f}},
        {FVector3f{-0.34f, 0.32f, -0.42f},
         FVector3f{0.22f, 0.28f, 0.26f},
         FVector4f{0.78f, 0.38f, 1.0f, 0.92f}},
    };

    points_.SetNumUninitialized(point_count);
    for (int32 index{0}; index < point_count; ++index) {
        auto const cluster_index{index % UE_ARRAY_COUNT(clusters)};
        auto const sample_index{index / UE_ARRAY_COUNT(clusters)};
        auto const angle{static_cast<float>(sample_index) * 2.399963f + cluster_index * 0.71f};
        auto const radial_fraction{
            FMath::Sqrt(FMath::Fmod(static_cast<float>(sample_index + 1) * 0.754877f, 1.0f))};
        auto const height_fraction{
            FMath::Fmod(static_cast<float>(sample_index + 1) * 0.569840f, 1.0f) * 2.0f - 1.0f};
        auto const horizontal_scale{radial_fraction *
                                    FMath::Sqrt(1.0f - height_fraction * height_fraction)};
        auto const& cluster{clusters[cluster_index]};
        auto position{cluster.centre +
                      FVector3f{FMath::Cos(angle) * horizontal_scale * cluster.extent.X,
                                FMath::Sin(angle) * horizontal_scale * cluster.extent.Y,
                                height_fraction * cluster.extent.Z}};
        auto color{cluster.color};

        if (index > 0 && index % 97 == 0) {
            position = FVector3f{
                -0.88f + 1.76f * FMath::Fmod(static_cast<float>(index) * 0.618034f, 1.0f),
                -0.88f + 1.76f * FMath::Fmod(static_cast<float>(index) * 0.414214f, 1.0f),
                -0.88f + 1.76f * FMath::Fmod(static_cast<float>(index) * 0.732051f, 1.0f)};
            color = FVector4f{1.0f, 0.94f, 0.70f, 1.0f};
        }

        points_[index] = {.position = position,
                          .size = 2.0f + static_cast<float>(index % 3) * 0.35f,
                          .color = color};
    }
}

auto SScatter3DWidget::initialise_output_texture() -> bool {
    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogScatter3DWidget, Error, TEXT("Failed to allocate the 3D scatter render target."));
        return false;
    }

    output_texture->ClearColor = FLinearColor{0.006f, 0.009f, 0.016f, 1.0f};
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(scatter_3d_output_texture_dimension,
                                     scatter_3d_output_texture_dimension,
                                     PF_R8G8B8A8,
                                     true);

    output_texture_.Reset(output_texture);
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}

void SScatter3DWidget::submit_render() {
    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogScatter3DWidget,
               Error,
               TEXT("Failed to acquire the 3D scatter render-target resource."));
        output_ready_ = false;
        return;
    }

    renderer_.render(points_, output_resource);
}
