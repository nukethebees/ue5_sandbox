#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

struct FScatter3DPoint;

struct FScatter3DBenchmarkOptions {
    TArray<int32> point_counts{1, 64, 1024, 16384, 65536};
    int32 warmup_iterations{3};
    int32 measured_iterations{20};
};

struct FScatter3DBenchmarkResult {
    FString stage;
    int32 point_count{0};
    int32 sample_count{0};
    double minimum_microseconds{0.0};
    double median_microseconds{0.0};
    double percentile_95_microseconds{0.0};
    double maximum_microseconds{0.0};
};

struct FScatter3DBenchmarkReport {
    TArray<FScatter3DBenchmarkResult> results;

    [[nodiscard]] auto to_csv() const -> FString;
    [[nodiscard]] auto to_text() const -> FString;
};

[[nodiscard]] auto run_scatter_3d_benchmark(FScatter3DBenchmarkOptions const& options)
    -> FScatter3DBenchmarkReport;

void benchmark_scatter_3d_rdg(TConstArrayView<FScatter3DPoint> points,
                              int32 warmup_iterations,
                              int32 measured_iterations,
                              TArray<double>& submission_samples,
                              TArray<double>& gpu_samples);
