#include "SandboxUI/EntityOverlay/EntityOverlayBenchmark.h"

#include "EntityOverlayRenderer.h"
#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"

namespace {
auto percentile(TConstArrayView<double> const samples, double const fraction) -> double {
    check(!samples.IsEmpty());
    auto const index{
        FMath::Clamp(FMath::FloorToInt((samples.Num() - 1) * fraction), 0, samples.Num() - 1)};
    return samples[index];
}

auto summarize(FString stage, int32 const count, TArray<double> samples)
    -> FEntityOverlayBenchmarkResult {
    samples.Sort();
    return {.stage = MoveTemp(stage),
            .candidate_count = count,
            .sample_count = samples.Num(),
            .minimum_microseconds = samples[0],
            .median_microseconds = percentile(samples, 0.5),
            .percentile_95_microseconds = percentile(samples, 0.95),
            .maximum_microseconds = samples.Last()};
}

auto make_source(int32 const count) -> TPair<TArray<FVector3f>, TArray<float>> {
    TArray<FVector3f> positions;
    TArray<float> health;
    positions.SetNumUninitialized(count);
    health.SetNumUninitialized(count);
    for (int32 index{0}; index < count; ++index) {
        positions[index] = {1000.0f + static_cast<float>(index % 100) * 10.0f,
                            static_cast<float>((index / 100) % 100) * 10.0f,
                            static_cast<float>(index % 17) * 5.0f};
        health[index] = static_cast<float>(index % 101) / 100.0f;
    }
    return {MoveTemp(positions), MoveTemp(health)};
}
} // namespace

auto FEntityOverlayBenchmarkReport::to_csv() const -> FString {
    FString output{TEXT("stage,candidates,samples,min_us,median_us,p95_us,max_us\n")};
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%s,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),
                                  *result.stage,
                                  result.candidate_count,
                                  result.sample_count,
                                  result.minimum_microseconds,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds,
                                  result.maximum_microseconds);
    }
    return output;
}

auto FEntityOverlayBenchmarkReport::to_text() const -> FString {
    FString output;
    for (auto const& result : results) {
        output += FString::Printf(TEXT("%6d candidates  %-20s median %8.2f us p95 %8.2f us\n"),
                                  result.candidate_count,
                                  *result.stage,
                                  result.median_microseconds,
                                  result.percentile_95_microseconds);
    }
    return output;
}

auto run_entity_overlay_benchmark(int32 const warmup_iterations, int32 const measured_iterations)
    -> FEntityOverlayBenchmarkReport {
    check(IsInGameThread());
    check(warmup_iterations >= 0);
    check(measured_iterations > 0);

    FEntityOverlayBenchmarkReport report;
    int32 const candidate_counts[]{100, 1000, 10000};
    for (auto const count : candidate_counts) {
        auto source{make_source(count)};
        TArray<FEntityOverlayInstance> collected;
        FEntityOverlayCollector collector;
        TArray<double> collection_samples;
        TArray<double> preparation_samples;
        collection_samples.Reserve(measured_iterations);
        preparation_samples.Reserve(measured_iterations);

        auto const total_iterations{warmup_iterations + measured_iterations};
        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            auto const start{FPlatformTime::Seconds()};
            collector.begin(FVector3f::ZeroVector, 100000.0f, collected);
            static_cast<void>(collector.append({source.Key, source.Value}));
            auto const elapsed{(FPlatformTime::Seconds() - start) * 1'000'000.0};
            if (iteration >= warmup_iterations) {
                collection_samples.Add(elapsed);
            }
        }

        FEntityOverlayFramePtr frame;
        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            auto const start{FPlatformTime::Seconds()};
            auto mutable_frame{MakeShared<FEntityOverlayFrame, ESPMode::ThreadSafe>()};
            mutable_frame->instances = collected;
            frame = MoveTemp(mutable_frame);
            auto const elapsed{(FPlatformTime::Seconds() - start) * 1'000'000.0};
            if (iteration >= warmup_iterations) {
                preparation_samples.Add(elapsed);
            }
        }

        TStrongObjectPtr<UTextureRenderTarget2D> output{NewObject<UTextureRenderTarget2D>()};
        check(output.IsValid());
        output->InitCustomFormat(1920, 1080, PF_R8G8B8A8, true);
        auto* const output_resource{output->GameThread_GetRenderTargetResource()};
        check(output_resource);
        FEntityOverlayView const view{.camera_origin = FVector3f::ZeroVector,
                                      .view_projection = FMatrix44f::Identity,
                                      .view_rect = FIntRect{0, 0, 1920, 1080},
                                      .output_size = {1920, 1080}};
        FEntityOverlayStyle const style;
        FEntityOverlayRenderer renderer;
        TArray<double> submission_samples;
        TArray<double> gpu_samples;
        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            auto const start{FPlatformTime::Seconds()};
            renderer.render(frame, view, style, output_resource);
            auto const elapsed{(FPlatformTime::Seconds() - start) * 1'000'000.0};
            if (iteration >= warmup_iterations) {
                submission_samples.Add(elapsed);
            }
        }
        FlushRenderingCommands();

        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            ENQUEUE_RENDER_COMMAND(MeasureEntityOverlayGPU)
            ([frame, view, style, output_resource, iteration, warmup_iterations, &gpu_samples](
                 FRHICommandListImmediate& rhi_command_list) {
                auto const measurement{measure_entity_overlay_gpu(
                    rhi_command_list, *frame, view, style, output_resource)};
                if (iteration >= warmup_iterations && measurement.IsSet()) {
                    gpu_samples.Add(measurement.GetValue());
                }
            });
        }
        FlushRenderingCommands();

        report.results.Add(
            summarize(TEXT("collection_filter"), count, MoveTemp(collection_samples)));
        report.results.Add(
            summarize(TEXT("packet_preparation"), count, MoveTemp(preparation_samples)));
        report.results.Add(
            summarize(TEXT("render_submission"), count, MoveTemp(submission_samples)));
        if (!gpu_samples.IsEmpty()) {
            report.results.Add(summarize(TEXT("gpu_upload_raster"), count, MoveTemp(gpu_samples)));
        }
    }
    return report;
}

auto write_entity_overlay_debug_image(FEntityOverlayFramePtr frame,
                                      FEntityOverlayView const& view,
                                      FString const& output_path) -> bool {
    check(IsInGameThread());
    if (!frame.IsValid() || !view.is_valid()) {
        return false;
    }

    TStrongObjectPtr<UTextureRenderTarget2D> output{NewObject<UTextureRenderTarget2D>()};
    if (!output.IsValid()) {
        return false;
    }
    output->InitCustomFormat(view.output_size.X, view.output_size.Y, PF_R8G8B8A8, true);
    auto* const output_resource{output->GameThread_GetRenderTargetResource()};
    if (output_resource == nullptr) {
        return false;
    }

    FEntityOverlayRenderer renderer;
    renderer.render(frame, view, FEntityOverlayStyle{}, output_resource);
    FlushRenderingCommands();
    renderer.render(frame, view, FEntityOverlayStyle{}, output_resource);
    FlushRenderingCommands();

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    TUniquePtr<FArchive> archive{IFileManager::Get().CreateFileWriter(*output_path)};
    return archive.IsValid() && FImageUtils::ExportRenderTarget2DAsPNG(output.Get(), *archive);
}
