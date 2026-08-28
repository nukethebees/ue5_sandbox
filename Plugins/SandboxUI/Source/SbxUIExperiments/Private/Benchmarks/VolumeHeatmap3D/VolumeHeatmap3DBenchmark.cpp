#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmark.h"

#include "Benchmarks/BenchmarkStatistics.h"

#include "Math/UnrealMathUtility.h"
#include "RenderingThread.h"
#include "VolumeHeatmap3D/VolumeHeatmap3DRenderer.h"

namespace ml::ui::volume_heatmap_3d_benchmark {
auto make_grid(int32 const dimension) -> FVolumeHeatmap3DGrid {
    FVolumeHeatmap3DGrid grid;
    grid.dimensions = {dimension, dimension, dimension};
    int32 const voxel_count{dimension * dimension * dimension};
    grid.values.SetNumUninitialized(voxel_count);
    for (int32 z{0}; z < dimension; ++z) {
        float const pz{static_cast<float>(z) / static_cast<float>(dimension - 1) * 2.0f - 1.0f};
        for (int32 y{0}; y < dimension; ++y) {
            float const py{static_cast<float>(y) / static_cast<float>(dimension - 1) * 2.0f - 1.0f};
            for (int32 x{0}; x < dimension; ++x) {
                float const px{static_cast<float>(x) / static_cast<float>(dimension - 1) * 2.0f -
                               1.0f};
                FVector3f const position{px, py, pz};
                auto gaussian = [position](FVector3f const centre, float const radius) {
                    auto const delta{position - centre};
                    return FMath::Exp(-delta.SizeSquared() / (2.0f * radius * radius));
                };
                float const density{gaussian({-0.42f, -0.25f, 0.20f}, 0.28f) * 0.95f +
                                    gaussian({0.38f, -0.12f, -0.30f}, 0.23f) * 0.88f +
                                    gaussian({0.12f, 0.42f, 0.28f}, 0.31f) * 0.72f};
                grid.values[x + dimension * (y + dimension * z)] =
                    FMath::Clamp(density, 0.0f, 1.0f);
            }
        }
    }
    return grid;
}

auto summarize(FString stage,
               int32 const grid_dimension,
               int32 const slice_count,
               TArray<double> samples) -> FVolumeHeatmap3DBenchmarkResult {
    check(!samples.IsEmpty());
    samples.Sort();
    return {.stage = MoveTemp(stage),
            .grid_dimension = grid_dimension,
            .voxel_count = grid_dimension * grid_dimension * grid_dimension,
            .slice_count = slice_count,
            .sample_count = samples.Num(),
            .minimum_microseconds = samples[0],
            .median_microseconds = ml::ui::benchmark::percentile(samples, 0.5),
            .percentile_95_microseconds = ml::ui::benchmark::percentile(samples, 0.95),
            .maximum_microseconds = samples.Last()};
}

void benchmark_case(FVolumeHeatmap3DBenchmarkReport& report,
                    FVolumeHeatmap3DBenchmarkOptions const& options,
                    int32 const grid_dimension,
                    int32 const slice_count) {
    auto const grid{make_grid(grid_dimension)};
    FVolumeHeatmap3DView view;
    view.slice_count = slice_count;
    TArray<double> submission_samples;
    TArray<double> gpu_samples;
    submission_samples.Reserve(options.measured_iterations);
    gpu_samples.Reserve(options.measured_iterations);
    benchmark_volume_heatmap_3d_rdg(grid,
                                    view,
                                    options.warmup_iterations,
                                    options.measured_iterations,
                                    submission_samples,
                                    gpu_samples);
    report.results.Add(summarize(
        TEXT("api_submission"), grid_dimension, slice_count, MoveTemp(submission_samples)));
    if (!gpu_samples.IsEmpty()) {
        report.results.Add(summarize(
            TEXT("gpu_upload_raster"), grid_dimension, slice_count, MoveTemp(gpu_samples)));
    }
}
} // namespace

auto FVolumeHeatmap3DBenchmarkReport::to_csv() const -> FString {
    FString output{
        TEXT("stage,grid_dimension,voxels,slices,samples,min_us,median_us,p95_us,max_us\n")};
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),
                                  *result.stage,
                                  result.grid_dimension,
                                  result.voxel_count,
                                  result.slice_count,
                                  result.sample_count,
                                  result.minimum_microseconds,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds,
                                  result.maximum_microseconds);
    }
    return output;
}

auto FVolumeHeatmap3DBenchmarkReport::to_text() const -> FString {
    FString output;
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%3d³  %3d slices  %-18s  median %8.2f us  p95 %8.2f us\n"),
                                  result.grid_dimension,
                                  result.slice_count,
                                  *result.stage,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds);
    }
    return output;
}

auto run_volume_heatmap_3d_benchmark(FVolumeHeatmap3DBenchmarkOptions const& options)
    -> FVolumeHeatmap3DBenchmarkReport {
    check(IsInGameThread());
    check(options.warmup_iterations >= 0);
    check(options.measured_iterations > 0);
    FVolumeHeatmap3DBenchmarkReport report;
    for (int32 const dimension : options.grid_dimensions) {
        if (dimension > 0 && dimension <= 128) {
            ml::ui::volume_heatmap_3d_benchmark::benchmark_case(
                report, options, dimension, options.fixed_slice_count);
        }
    }
    for (int32 const slice_count : options.slice_counts) {
        if (slice_count >= 8 && slice_count <= 256) {
            ml::ui::volume_heatmap_3d_benchmark::benchmark_case(
                report, options, options.fixed_grid_dimension, slice_count);
        }
    }
    return report;
}
