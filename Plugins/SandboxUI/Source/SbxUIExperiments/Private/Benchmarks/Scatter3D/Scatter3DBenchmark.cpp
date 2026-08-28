#include "Benchmarks/Scatter3D/Scatter3DBenchmark.h"

#include "Math/UnrealMathUtility.h"
#include "RenderingThread.h"
#include "Scatter3D/Scatter3DRenderer.h"

namespace {
auto make_scatter_3d_points(int32 const point_count) -> TArray<FScatter3DPoint> {
    FVector4f const colors[]{
        FVector4f{0.12f, 0.72f, 1.0f, 0.92f},
        FVector4f{1.0f, 0.42f, 0.12f, 0.92f},
        FVector4f{0.48f, 1.0f, 0.30f, 0.92f},
        FVector4f{0.78f, 0.38f, 1.0f, 0.92f},
    };

    TArray<FScatter3DPoint> points;
    points.SetNumUninitialized(point_count);
    for (int32 index{0}; index < point_count; ++index) {
        auto const x{-0.88f + 1.76f * FMath::Fmod(static_cast<float>(index + 1) * 0.618034f, 1.0f)};
        auto const y{-0.88f + 1.76f * FMath::Fmod(static_cast<float>(index + 1) * 0.414214f, 1.0f)};
        auto const z{-0.88f + 1.76f * FMath::Fmod(static_cast<float>(index + 1) * 0.732051f, 1.0f)};
        points[index] = {.position = FVector3f{x, y, z},
                         .size = 2.0f + static_cast<float>(index % 3) * 0.35f,
                         .color = colors[index % UE_ARRAY_COUNT(colors)]};
    }
    return points;
}

auto scatter_3d_percentile(TArray<double> const& sorted_samples, double const fraction) -> double {
    check(!sorted_samples.IsEmpty());
    auto const index{FMath::Clamp(
        FMath::CeilToInt(fraction * sorted_samples.Num()) - 1, 0, sorted_samples.Num() - 1)};
    return sorted_samples[index];
}

auto summarize_scatter_3d(FString stage, int32 const point_count, TArray<double> samples)
    -> FScatter3DBenchmarkResult {
    check(!samples.IsEmpty());
    samples.Sort();
    return {.stage = MoveTemp(stage),
            .point_count = point_count,
            .sample_count = samples.Num(),
            .minimum_microseconds = samples[0],
            .median_microseconds = scatter_3d_percentile(samples, 0.5),
            .percentile_95_microseconds = scatter_3d_percentile(samples, 0.95),
            .maximum_microseconds = samples.Last()};
}
} // namespace

auto FScatter3DBenchmarkReport::to_csv() const -> FString {
    FString output{TEXT("stage,points,samples,min_us,median_us,p95_us,max_us\n")};
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%s,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),
                                  *result.stage,
                                  result.point_count,
                                  result.sample_count,
                                  result.minimum_microseconds,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds,
                                  result.maximum_microseconds);
    }
    return output;
}

auto FScatter3DBenchmarkReport::to_text() const -> FString {
    FString output;
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%6d points  %-18s  median %8.2f us  p95 %8.2f us\n"),
                                  result.point_count,
                                  *result.stage,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds);
    }
    return output;
}

auto run_scatter_3d_benchmark(FScatter3DBenchmarkOptions const& options)
    -> FScatter3DBenchmarkReport {
    check(IsInGameThread());
    check(options.warmup_iterations >= 0);
    check(options.measured_iterations > 0);

    FScatter3DBenchmarkReport report;
    for (auto const point_count : options.point_counts) {
        if (point_count <= 0) {
            continue;
        }

        auto const points{make_scatter_3d_points(point_count)};
        TArray<double> submission_samples;
        TArray<double> gpu_samples;
        submission_samples.Reserve(options.measured_iterations);
        gpu_samples.Reserve(options.measured_iterations);
        benchmark_scatter_3d_rdg(points,
                                 options.warmup_iterations,
                                 options.measured_iterations,
                                 submission_samples,
                                 gpu_samples);
        report.results.Add(summarize_scatter_3d(
            TEXT("api_submission"), point_count, MoveTemp(submission_samples)));
        if (!gpu_samples.IsEmpty()) {
            report.results.Add(summarize_scatter_3d(
                TEXT("gpu_upload_raster"), point_count, MoveTemp(gpu_samples)));
        }
    }
    return report;
}
