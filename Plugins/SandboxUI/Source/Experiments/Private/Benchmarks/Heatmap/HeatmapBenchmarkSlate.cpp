#include "Benchmarks/Heatmap/HeatmapBenchmark.h"

#include "HAL/PlatformTime.h"
#include "SandboxUI/widgets/SHeatmap2D.h"

namespace {
auto make_slate_grid(TConstArrayView<float> const values, int32 const resolution) -> FHeatmapGrid {
    FHeatmapGrid grid{.columns = resolution, .rows = resolution};
    grid.values.Append(values.GetData(), values.Num());
    return grid;
}

void prepare_slate_heatmap(SHeatmap2D const& widget,
                           TConstArrayView<FLinearColor> const color_lut) {
    auto const cells{build_heatmap_cell_geometry(
        widget.get_grid(), widget.get_value_range(), color_lut, FVector2f{512.0f, 512.0f})};
    [[maybe_unused]] auto const batches{build_heatmap_mesh_batches(cells, FVector2f::ZeroVector)};
}
}

void benchmark_slate_heatmap(TConstArrayView<float> const values,
                             int32 const resolution,
                             int32 const warmup_iterations,
                             int32 const measured_iterations,
                             TArray<double>& submission_samples,
                             TArray<double>& preparation_samples) {
    FHeatmap2DStyle style;
    style.desired_size = {512.0f, 512.0f};
    style.chart_padding = FMargin{0.0f};
    style.show_axes = false;
    auto widget{SNew(SHeatmap2D).Style(style)};
    auto const color_lut{build_heatmap_color_lut(style.color_stops)};

    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        check(widget->set_grid(make_slate_grid(values, resolution)));
        prepare_slate_heatmap(*widget, color_lut);
    }

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        auto grid{make_slate_grid(values, resolution)};
        auto const submission_start_seconds{FPlatformTime::Seconds()};
        check(widget->set_grid(MoveTemp(grid)));
        submission_samples.Add((FPlatformTime::Seconds() - submission_start_seconds) * 1'000'000.0);

        auto const preparation_start_seconds{FPlatformTime::Seconds()};
        prepare_slate_heatmap(*widget, color_lut);
        preparation_samples.Add((FPlatformTime::Seconds() - preparation_start_seconds) *
                                1'000'000.0);
    }
}
