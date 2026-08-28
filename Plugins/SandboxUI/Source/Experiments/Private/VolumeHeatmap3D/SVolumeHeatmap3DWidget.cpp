#include "SVolumeHeatmap3DWidget.h"

#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolumeHeatmap3DWidget, Log, All);

namespace {
constexpr int32 output_texture_dimension{512};

auto gaussian(FVector3f const position, FVector3f const centre, float const radius) -> float {
    auto const delta{position - centre};
    return FMath::Exp(-delta.SizeSquared() / (2.0f * radius * radius));
}
}

void SVolumeHeatmap3DWidget::Construct(FArguments const&) {
    initialise_grid();
    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
    brush_.ImageSize = FVector2D{output_texture_dimension, output_texture_dimension};
    output_ready_ = initialise_output_texture();
    SetCanTick(true);
    ChildSlot[SAssignNew(image_, SImage).Image(&brush_)];
}

SVolumeHeatmap3DWidget::~SVolumeHeatmap3DWidget() {
    brush_.SetResourceObject(nullptr);
    image_.Reset();
    output_texture_.Reset();
}

void SVolumeHeatmap3DWidget::set_pattern(EVolumeHeatmap3DPattern const pattern) {
    pattern_ = pattern;
    initialise_grid();
    request_render();
}

void SVolumeHeatmap3DWidget::set_grid_dimension(int32 const dimension) {
    check(dimension == 16 || dimension == 32 || dimension == 64 || dimension == 128);
    grid_.dimensions = FIntVector{dimension};
    initialise_grid();
    request_render();
}

void SVolumeHeatmap3DWidget::set_slice_count(int32 const slice_count) {
    view_.slice_count = FMath::Clamp(slice_count, 8, 256);
    request_render();
}

void SVolumeHeatmap3DWidget::set_yaw(float const yaw_degrees) {
    view_.yaw_degrees = yaw_degrees;
    request_render();
}

void SVolumeHeatmap3DWidget::set_pitch(float const pitch_degrees) {
    view_.pitch_degrees = FMath::Clamp(pitch_degrees, -85.0f, 85.0f);
    request_render();
}

void SVolumeHeatmap3DWidget::set_density_scale(float const density_scale) {
    view_.density_scale = FMath::Clamp(density_scale, 0.25f, 8.0f);
    request_render();
}

void SVolumeHeatmap3DWidget::Tick(FGeometry const& allotted_geometry,
                                  double const current_time,
                                  float const delta_time) {
    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);
    if (initial_render_delay_ticks_ == 0 || --initial_render_delay_ticks_ > 0) {
        return;
    }
    initial_render_delay_ticks_ = 0;
    SetCanTick(false);
    request_render();
}

void SVolumeHeatmap3DWidget::initialise_grid() {
    auto const dimensions{grid_.dimensions};
    int32 const voxel_count{dimensions.X * dimensions.Y * dimensions.Z};
    grid_.values.SetNumUninitialized(voxel_count);
    for (int32 z{0}; z < dimensions.Z; ++z) {
        float const pz{static_cast<float>(z) / static_cast<float>(dimensions.Z - 1) * 2.0f - 1.0f};
        for (int32 y{0}; y < dimensions.Y; ++y) {
            float const py{static_cast<float>(y) / static_cast<float>(dimensions.Y - 1) * 2.0f -
                           1.0f};
            for (int32 x{0}; x < dimensions.X; ++x) {
                float const px{static_cast<float>(x) / static_cast<float>(dimensions.X - 1) * 2.0f -
                               1.0f};
                FVector3f const position{px, py, pz};
                float density{0.0f};
                if (pattern_ == EVolumeHeatmap3DPattern::GaussianClouds) {
                    density = gaussian(position, {-0.42f, -0.25f, 0.20f}, 0.28f) * 0.95f +
                              gaussian(position, {0.38f, -0.12f, -0.30f}, 0.23f) * 0.88f +
                              gaussian(position, {0.12f, 0.42f, 0.28f}, 0.31f) * 0.72f +
                              gaussian(position, {-0.28f, 0.34f, -0.38f}, 0.19f) * 0.65f;
                } else {
                    float const radius{position.Size()};
                    float const shell_delta{(radius - 0.62f) / 0.075f};
                    density = FMath::Exp(-0.5f * shell_delta * shell_delta) * 0.92f;
                    density += gaussian(position, {0.24f, -0.18f, 0.10f}, 0.16f) * 0.55f;
                }
                int32 const index{x + dimensions.X * (y + dimensions.Y * z)};
                grid_.values[index] = FMath::Clamp(density, 0.0f, 1.0f);
            }
        }
    }
}

auto SVolumeHeatmap3DWidget::initialise_output_texture() -> bool {
    auto* const output_texture{NewObject<UTextureRenderTarget2D>()};
    if (output_texture == nullptr) {
        UE_LOG(LogVolumeHeatmap3DWidget,
               Error,
               TEXT("Failed to allocate the volume heatmap render target."));
        return false;
    }
    output_texture->ClearColor = FLinearColor{0.006f, 0.009f, 0.016f, 1.0f};
    output_texture->Filter = TF_Bilinear;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(
        output_texture_dimension, output_texture_dimension, PF_R8G8B8A8, true);
    output_texture_.Reset(output_texture);
    brush_.SetResourceObject(output_texture_.Get());
    return true;
}

void SVolumeHeatmap3DWidget::request_render() {
    if (!output_ready_ || GUsingNullRHI || initial_render_delay_ticks_ != 0) {
        return;
    }
    submit_render();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SVolumeHeatmap3DWidget::submit_render() {
    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogVolumeHeatmap3DWidget,
               Error,
               TEXT("Failed to acquire the volume heatmap render-target resource."));
        output_ready_ = false;
        return;
    }
    renderer_.render(grid_, view_, output_resource);
}
