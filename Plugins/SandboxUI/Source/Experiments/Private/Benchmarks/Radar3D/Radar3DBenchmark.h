#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

struct FRadar3DContact;

struct FRadar3DBenchmarkOptions {
    TArray<int32> contact_counts{1, 4, 16, 64, 256};
    int32 warmup_iterations{3};
    int32 measured_iterations{20};
};

struct FRadar3DBenchmarkResult {
    FString stage;
    int32 contact_count{0};
    int32 sample_count{0};
    double minimum_microseconds{0.0};
    double median_microseconds{0.0};
    double percentile_95_microseconds{0.0};
    double maximum_microseconds{0.0};
};

struct FRadar3DBenchmarkReport {
    TArray<FRadar3DBenchmarkResult> results;

    [[nodiscard]] auto to_csv() const -> FString;
    [[nodiscard]] auto to_text() const -> FString;
};

[[nodiscard]] auto run_radar_3d_benchmark(FRadar3DBenchmarkOptions const& options)
    -> FRadar3DBenchmarkReport;

void benchmark_radar_3d_rdg(TConstArrayView<FRadar3DContact> contacts,
                            int32 warmup_iterations,
                            int32 measured_iterations,
                            TArray<double>& submission_samples,
                            TArray<double>& gpu_samples);
