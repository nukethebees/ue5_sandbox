#include "Benchmarks/Heatmap/HeatmapBenchmark.h"

#include "HAL/PlatformTime.h"
#include "SandboxUI/widgets/SHeatmap2D.h"

#include "generated/HeatmapBenchmark.slate.generated.h"

namespace {
auto make_slate_grid(TConstArrayView<float> const values, int32 const resolution) -> FHeatmapGrid {
    FHeatmapGrid grid{.columns = resolution, .rows = resolution};
    grid.values.reserve(static_cast<std::size_t>(values.Num()));
    for (auto const value : values) {
        grid.values.push_back(value);
    }
    return grid;
}

void prepare_slate_heatmap(SHeatmap2D const& widget,
                           std::span<ml::ui::Color4f const> const color_lut) {
    auto const cells{ml::ui::heatmap_2d::build_cell_geometry(
        widget.get_grid(), widget.get_value_range(), color_lut, {512.0f, 512.0f})};
    [[maybe_unused]] auto const batches{ml::ui::heatmap_2d::build_mesh_batches(cells, {})};
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
    auto widget{SlateGenerated::HeatmapBenchmark::BuildHeatmap(style)};
    std::vector<ml::ui::heatmap_2d::ColorStop> color_stops;
    color_stops.reserve(static_cast<std::size_t>(style.color_stops.Num()));
    for (auto const& stop : style.color_stops) {
        color_stops.push_back(
            {stop.position, {stop.color.R, stop.color.G, stop.color.B, stop.color.A}});
    }
    auto const color_lut{ml::ui::heatmap_2d::build_color_lut(color_stops)};

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
