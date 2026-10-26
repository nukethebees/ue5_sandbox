#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

struct FVolumeHeatmap3DGrid;
struct FVolumeHeatmap3DView;

struct FVolumeHeatmap3DBenchmarkOptions {
    TArray<int32> grid_dimensions{16, 32, 64, 128};
    TArray<int32> slice_counts{16, 32, 64, 128, 256};
    int32 fixed_grid_dimension{64};
    int32 fixed_slice_count{96};
    int32 warmup_iterations{3};
    int32 measured_iterations{20};
};

struct FVolumeHeatmap3DBenchmarkResult {
    FString stage;
    int32 grid_dimension{0};
    int32 voxel_count{0};
    int32 slice_count{0};
    int32 sample_count{0};
    double minimum_microseconds{0.0};
    double median_microseconds{0.0};
    double percentile_95_microseconds{0.0};
    double maximum_microseconds{0.0};
};

struct FVolumeHeatmap3DBenchmarkReport {
    TArray<FVolumeHeatmap3DBenchmarkResult> results;

    [[nodiscard]] auto to_csv() const -> FString;
    [[nodiscard]] auto to_text() const -> FString;
};

[[nodiscard]] auto run_volume_heatmap_3d_benchmark(FVolumeHeatmap3DBenchmarkOptions const& options)
    -> FVolumeHeatmap3DBenchmarkReport;

void benchmark_volume_heatmap_3d_rdg(FVolumeHeatmap3DGrid const& grid,
                                     FVolumeHeatmap3DView view,
                                     int32 warmup_iterations,
                                     int32 measured_iterations,
                                     TArray<double>& submission_samples,
                                     TArray<double>& gpu_samples);
