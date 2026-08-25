#include "Benchmarks/Heatmap/HeatmapBenchmark.h"

#include "Math/UnrealMathUtility.h"
#include "RenderingThread.h"

namespace {
auto make_grid_values(int32 const resolution) -> TArray<float> {
    TArray<float> values;
    values.SetNumUninitialized(resolution * resolution);

    auto const gaussian = [](float const x,
                             float const y,
                             float const centre_x,
                             float const centre_y,
                             float const radius) {
        auto const delta_x{x - centre_x};
        auto const delta_y{y - centre_y};
        return FMath::Exp(-(delta_x * delta_x + delta_y * delta_y) / (2.0f * radius * radius));
    };

    for (int32 y{0}; y < resolution; ++y) {
        auto const normalized_y{(static_cast<float>(y) + 0.5f) / resolution};
        for (int32 x{0}; x < resolution; ++x) {
            auto const normalized_x{(static_cast<float>(x) + 0.5f) / resolution};
            values[y * resolution + x] =
                FMath::Clamp(0.95f * gaussian(normalized_x, normalized_y, 0.28f, 0.32f, 0.10f) +
                                 0.75f * gaussian(normalized_x, normalized_y, 0.68f, 0.42f, 0.14f) +
                                 0.60f * gaussian(normalized_x, normalized_y, 0.48f, 0.78f, 0.08f) +
                                 0.18f * normalized_x + 0.08f * normalized_y,
                             0.0f,
                             1.0f);
        }
    }
    return values;
}

auto percentile(TArray<double> const& sorted_samples, double const fraction) -> double {
    check(!sorted_samples.IsEmpty());
    auto const index{FMath::Clamp(
        FMath::CeilToInt(fraction * sorted_samples.Num()) - 1, 0, sorted_samples.Num() - 1)};
    return sorted_samples[index];
}

auto summarize(FString backend, FString stage, int32 const resolution, TArray<double> samples)
    -> FHeatmapBenchmarkResult {
    check(!samples.IsEmpty());
    samples.Sort();
    return {.backend = MoveTemp(backend),
            .stage = MoveTemp(stage),
            .resolution = resolution,
            .sample_count = samples.Num(),
            .minimum_microseconds = samples[0],
            .median_microseconds = percentile(samples, 0.5),
            .percentile_95_microseconds = percentile(samples, 0.95),
            .maximum_microseconds = samples.Last()};
}
}

auto FHeatmapBenchmarkReport::to_csv() const -> FString {
    FString output{TEXT("backend,stage,resolution,samples,min_us,median_us,p95_us,max_us\n")};
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%s,%s,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),
                                  *result.backend,
                                  *result.stage,
                                  result.resolution,
                                  result.sample_count,
                                  result.minimum_microseconds,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds,
                                  result.maximum_microseconds);
    }
    return output;
}

auto FHeatmapBenchmarkReport::to_text() const -> FString {
    FString output;
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%-8s  %3dx%-3d  %-18s  median %8.2f us  p95 %8.2f us\n"),
                                  *result.backend,
                                  result.resolution,
                                  result.resolution,
                                  *result.stage,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds);
    }
    return output;
}

auto run_heatmap_benchmark(FHeatmapBenchmarkOptions const& options) -> FHeatmapBenchmarkReport {
    check(IsInGameThread());
    check(options.warmup_iterations >= 0);
    check(options.measured_iterations > 0);

    FHeatmapBenchmarkReport report;
    for (auto const resolution : options.resolutions) {
        if (resolution <= 0 || resolution > 512) {
            continue;
        }

        auto const values{make_grid_values(resolution)};
        TArray<double> rdg_submission_samples;
        TArray<double> rdg_gpu_samples;
        rdg_submission_samples.Reserve(options.measured_iterations);
        rdg_gpu_samples.Reserve(options.measured_iterations);
        benchmark_rdg_heatmap(values,
                              resolution,
                              options.warmup_iterations,
                              options.measured_iterations,
                              rdg_submission_samples,
                              rdg_gpu_samples);
        report.results.Add(summarize(
            TEXT("RDG"), TEXT("api_submission"), resolution, MoveTemp(rdg_submission_samples)));
        if (!rdg_gpu_samples.IsEmpty()) {
            report.results.Add(summarize(
                TEXT("RDG"), TEXT("gpu_upload_compute"), resolution, MoveTemp(rdg_gpu_samples)));
        }

        TArray<double> slate_submission_samples;
        TArray<double> slate_preparation_samples;
        slate_submission_samples.Reserve(options.measured_iterations);
        slate_preparation_samples.Reserve(options.measured_iterations);
        benchmark_slate_heatmap(values,
                                resolution,
                                options.warmup_iterations,
                                options.measured_iterations,
                                slate_submission_samples,
                                slate_preparation_samples);
        report.results.Add(summarize(
            TEXT("Slate"), TEXT("api_submission"), resolution, MoveTemp(slate_submission_samples)));
        report.results.Add(summarize(TEXT("Slate"),
                                     TEXT("geometry_batches"),
                                     resolution,
                                     MoveTemp(slate_preparation_samples)));
    }
    return report;
}
