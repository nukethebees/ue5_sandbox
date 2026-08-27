#include "Benchmarks/Radar3D/Radar3DBenchmark.h"

#include "Math/UnrealMathUtility.h"
#include "Radar3D/Radar3DRenderer.h"
#include "RenderingThread.h"

namespace {
auto make_contacts(int32 const contact_count) -> TArray<FRadar3DContact> {
    FVector4f const colors[]{
        FVector4f{1.0f, 0.28f, 0.12f, 1.0f},
        FVector4f{0.2f, 0.85f, 1.0f, 1.0f},
        FVector4f{0.42f, 1.0f, 0.38f, 1.0f},
        FVector4f{1.0f, 0.88f, 0.22f, 1.0f},
        FVector4f{0.75f, 0.38f, 1.0f, 1.0f},
    };

    TArray<FRadar3DContact> contacts;
    contacts.SetNumUninitialized(contact_count);
    for (int32 index{0}; index < contact_count; ++index) {
        auto const fraction{static_cast<float>(index) / static_cast<float>(contact_count)};
        auto const radius{0.18f + 0.68f * FMath::Fmod(index * 0.618034f, 1.0f)};
        auto const angle{fraction * 17.0f + index * 0.37f};
        contacts[index] = {
            .position = FVector3f{radius * FMath::Cos(angle),
                                  radius * FMath::Sin(angle),
                                  -0.8f + 1.6f * FMath::Fmod(index * 0.414214f, 1.0f)},
            .size = 4.5f + static_cast<float>(index % 4),
            .color = colors[index % UE_ARRAY_COUNT(colors)],
        };
    }
    return contacts;
}

auto radar_percentile(TArray<double> const& sorted_samples, double const fraction) -> double {
    check(!sorted_samples.IsEmpty());
    auto const index{FMath::Clamp(
        FMath::CeilToInt(fraction * sorted_samples.Num()) - 1, 0, sorted_samples.Num() - 1)};
    return sorted_samples[index];
}

auto summarize(FString stage, int32 const contact_count, TArray<double> samples)
    -> FRadar3DBenchmarkResult {
    check(!samples.IsEmpty());
    samples.Sort();
    return {.stage = MoveTemp(stage),
            .contact_count = contact_count,
            .sample_count = samples.Num(),
            .minimum_microseconds = samples[0],
            .median_microseconds = radar_percentile(samples, 0.5),
            .percentile_95_microseconds = radar_percentile(samples, 0.95),
            .maximum_microseconds = samples.Last()};
}
} // namespace

auto FRadar3DBenchmarkReport::to_csv() const -> FString {
    FString output{TEXT("stage,contacts,samples,min_us,median_us,p95_us,max_us\n")};
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%s,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),
                                  *result.stage,
                                  result.contact_count,
                                  result.sample_count,
                                  result.minimum_microseconds,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds,
                                  result.maximum_microseconds);
    }
    return output;
}

auto FRadar3DBenchmarkReport::to_text() const -> FString {
    FString output;
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%4d contacts  %-18s  median %8.2f us  p95 %8.2f us\n"),
                                  result.contact_count,
                                  *result.stage,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds);
    }
    return output;
}

auto run_radar_3d_benchmark(FRadar3DBenchmarkOptions const& options) -> FRadar3DBenchmarkReport {
    check(IsInGameThread());
    check(options.warmup_iterations >= 0);
    check(options.measured_iterations > 0);

    FRadar3DBenchmarkReport report;
    for (auto const contact_count : options.contact_counts) {
        if (contact_count <= 0) {
            continue;
        }

        auto const contacts{make_contacts(contact_count)};
        TArray<double> submission_samples;
        TArray<double> gpu_samples;
        submission_samples.Reserve(options.measured_iterations);
        gpu_samples.Reserve(options.measured_iterations);
        benchmark_radar_3d_rdg(contacts,
                               options.warmup_iterations,
                               options.measured_iterations,
                               submission_samples,
                               gpu_samples);
        report.results.Add(
            summarize(TEXT("api_submission"), contact_count, MoveTemp(submission_samples)));
        if (!gpu_samples.IsEmpty()) {
            report.results.Add(
                summarize(TEXT("gpu_upload_raster"), contact_count, MoveTemp(gpu_samples)));
        }
    }
    return report;
}
