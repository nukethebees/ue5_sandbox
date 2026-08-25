#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

struct FHeatmapBenchmarkOptions {
    TArray<int32> resolutions{32, 64, 128, 256, 512};
    int32 warmup_iterations{3};
    int32 measured_iterations{20};
};

struct FHeatmapBenchmarkResult {
    FString backend;
    FString stage;
    int32 resolution{0};
    int32 sample_count{0};
    double minimum_microseconds{0.0};
    double median_microseconds{0.0};
    double percentile_95_microseconds{0.0};
    double maximum_microseconds{0.0};
};

struct FHeatmapBenchmarkReport {
    TArray<FHeatmapBenchmarkResult> results;

    [[nodiscard]] auto to_csv() const -> FString;
    [[nodiscard]] auto to_text() const -> FString;
};

[[nodiscard]] auto run_heatmap_benchmark(FHeatmapBenchmarkOptions const& options)
    -> FHeatmapBenchmarkReport;

void benchmark_rdg_heatmap(TConstArrayView<float> values,
                           int32 resolution,
                           int32 warmup_iterations,
                           int32 measured_iterations,
                           TArray<double>& submission_samples,
                           TArray<double>& gpu_samples);

void benchmark_slate_heatmap(TConstArrayView<float> values,
                             int32 resolution,
                             int32 warmup_iterations,
                             int32 measured_iterations,
                             TArray<double>& submission_samples,
                             TArray<double>& preparation_samples);
