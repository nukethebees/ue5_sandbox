#include "SbxUIExperiments/HeatmapRDG/HeatmapRDGWidget.h"

#include "HeatmapRDGRenderer.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Internationalization/Text.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeatmapRDGWidget, Log, All);

namespace ml::ui::heatmap_rdg {
constexpr int32 output_texture_dimension{512};

auto maximum_heatmap_dimension() -> int32 {
    return FMath::Min(output_texture_dimension, static_cast<int32>(GetMax2DTextureDimension()));
}

auto are_supported_heatmap_dimensions(int32 const width, int32 const height) -> bool {
    return width <= maximum_heatmap_dimension() && height <= maximum_heatmap_dimension();
}
}

auto FHeatmapRDGGrid::is_valid() const noexcept -> bool {
    if (width <= 0 || height <= 0) {
        return false;
    }

    auto const expected_value_count{static_cast<int64>(width) * static_cast<int64>(height)};
    return expected_value_count == static_cast<int64>(values.Num());
}

UHeatmapRDGWidget::UHeatmapRDGWidget() {
    brush_.DrawAs = ESlateBrushDrawType::Image;
    brush_.ImageType = ESlateBrushImageType::FullColor;
    brush_.Tiling = ESlateBrushTileType::NoTile;
}

bool UHeatmapRDGWidget::set_grid(FHeatmapRDGGrid const& grid) {
    check(IsInGameThread());

    if (!grid.is_valid()) {
        UE_LOG(LogHeatmapRDGWidget,
               Warning,
               TEXT("Rejected heatmap grid %dx%d with %d values."),
               grid.width,
               grid.height,
               grid.values.Num());
        return false;
    }
    if (!ml::ui::heatmap_rdg::are_supported_heatmap_dimensions(grid.width, grid.height)) {
        UE_LOG(LogHeatmapRDGWidget,
               Warning,
               TEXT("Rejected heatmap grid %dx%d because the experiment supports at most %dx%d."),
               grid.width,
               grid.height,
               ml::ui::heatmap_rdg::maximum_heatmap_dimension(),
               ml::ui::heatmap_rdg::maximum_heatmap_dimension());
        return false;
    }

    auto const grid_dimensions{FIntPoint{grid.width, grid.height}};
    auto const output_dimensions{FIntPoint{ml::ui::heatmap_rdg::output_texture_dimension,
                                           ml::ui::heatmap_rdg::output_texture_dimension}};
    if (!ensure_output_texture(output_dimensions)) {
        return false;
    }

    auto const uv_max{
        FVector2f{static_cast<float>(grid.width) / ml::ui::heatmap_rdg::output_texture_dimension,
                  static_cast<float>(grid.height) / ml::ui::heatmap_rdg::output_texture_dimension}};
    brush_.SetUVRegion(FBox2f{FVector2f::ZeroVector, uv_max});
    brush_.ImageSize = FVector2D{grid_dimensions};
    if (image_.IsValid()) {
        image_->SetImage(static_cast<FSlateBrush const*>(nullptr));
        image_->SetImage(&brush_);
    }

    auto* const output_resource{output_texture_->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        UE_LOG(LogHeatmapRDGWidget,
               Error,
               TEXT("Failed to acquire the heatmap render-target resource."));
        return false;
    }

    TArray<float> values_snapshot{grid.values};
    ENQUEUE_RENDER_COMMAND(RenderHeatmapRDG)
    ([values = MoveTemp(values_snapshot), grid_dimensions, output_resource](
         FRHICommandListImmediate& rhi_command_list) mutable {
        render_heatmap_rdg(rhi_command_list, MoveTemp(values), grid_dimensions, output_resource);
    });

    has_submitted_grid_ = true;
    return true;
}

void UHeatmapRDGWidget::generate_demo_grid(int32 const width, int32 const height) {
    FHeatmapRDGGrid grid{.width = width, .height = height};
    if (width <= 0 || height <= 0) {
        [[maybe_unused]] auto const submitted{set_grid(grid)};
        return;
    }
    if (!ml::ui::heatmap_rdg::are_supported_heatmap_dimensions(width, height)) {
        UE_LOG(LogHeatmapRDGWidget,
               Warning,
               TEXT("Rejected demo heatmap %dx%d because the experiment supports at most %dx%d."),
               width,
               height,
               ml::ui::heatmap_rdg::maximum_heatmap_dimension(),
               ml::ui::heatmap_rdg::maximum_heatmap_dimension());
        return;
    }

    auto const value_count{static_cast<int64>(width) * static_cast<int64>(height)};
    if (value_count > MAX_int32) {
        UE_LOG(LogHeatmapRDGWidget,
               Warning,
               TEXT("Rejected demo heatmap dimensions %dx%d because the grid is too large."),
               width,
               height);
        return;
    }

    grid.values.SetNumUninitialized(static_cast<int32>(value_count));
    auto const gaussian = [](float const x,
                             float const y,
                             float const centre_x,
                             float const centre_y,
                             float const radius) {
        auto const delta_x{x - centre_x};
        auto const delta_y{y - centre_y};
        return FMath::Exp(-(delta_x * delta_x + delta_y * delta_y) / (2.0f * radius * radius));
    };

    auto const row_count{height};
    auto const column_count{width};
    for (int32 y{0}; y < row_count; ++y) {
        auto const normalized_y{(static_cast<float>(y) + 0.5f) / static_cast<float>(height)};
        for (int32 x{0}; x < column_count; ++x) {
            auto const normalized_x{(static_cast<float>(x) + 0.5f) / static_cast<float>(width)};
            auto const hotspots{0.95f * gaussian(normalized_x, normalized_y, 0.28f, 0.32f, 0.10f) +
                                0.75f * gaussian(normalized_x, normalized_y, 0.68f, 0.42f, 0.14f) +
                                0.60f * gaussian(normalized_x, normalized_y, 0.48f, 0.78f, 0.08f)};
            auto const gradient{0.18f * normalized_x + 0.08f * normalized_y};
            grid.values[y * width + x] = hotspots + gradient;
        }
    }

    [[maybe_unused]] auto const submitted{set_grid(grid)};
}

TSharedRef<SWidget> UHeatmapRDGWidget::RebuildWidget() {
    SAssignNew(image_, SImage).Image(&brush_);
    if (!has_submitted_grid_ && !GUsingNullRHI) {
        generate_demo_grid(128, 128);
    }
    return image_.ToSharedRef();
}

void UHeatmapRDGWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    image_.Reset();
}

#if WITH_EDITOR
const FText UHeatmapRDGWidget::GetPaletteCategory() {
    return FText::FromString(TEXT("Sandbox Experiments"));
}
#endif

auto UHeatmapRDGWidget::ensure_output_texture(FIntPoint const dimensions) -> bool {
    if (output_texture_ != nullptr && output_size_ == dimensions) {
        return true;
    }

    auto* const output_texture{NewObject<UTextureRenderTarget2D>(this)};
    if (output_texture == nullptr) {
        UE_LOG(LogHeatmapRDGWidget, Error, TEXT("Failed to allocate the heatmap render target."));
        return false;
    }

    output_texture->bSupportsUAV = true;
    output_texture->ClearColor = FLinearColor::Black;
    output_texture->Filter = TF_Nearest;
    output_texture->AddressX = TA_Clamp;
    output_texture->AddressY = TA_Clamp;
    output_texture->InitCustomFormat(dimensions.X, dimensions.Y, PF_R8G8B8A8, true);

    output_texture_ = output_texture;
    output_size_ = dimensions;
    brush_.SetResourceObject(output_texture_);
    if (image_.IsValid()) {
        image_->SetImage(static_cast<FSlateBrush const*>(nullptr));
        image_->SetImage(&brush_);
    }
    return true;
}
