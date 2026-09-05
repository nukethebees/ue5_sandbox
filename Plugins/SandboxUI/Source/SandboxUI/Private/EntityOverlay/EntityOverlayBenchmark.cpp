#include "SandboxUI/EntityOverlay/EntityOverlayBenchmark.h"

#include "EntityOverlayRenderer.h"
#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"
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

struct FBenchmarkSource {
    TArray<FVector3f> positions;
    TArray<float> health;
    TArray<float> world_radii;
};

enum class EBenchmarkVisibility : uint8 {
    Visible,
    Culled,
    Mixed,
};

auto make_source(int32 const count, EBenchmarkVisibility const visibility) -> FBenchmarkSource {
    TArray<FVector3f> positions;
    TArray<float> health;
    TArray<float> world_radii;
    positions.SetNumUninitialized(count);
    health.SetNumUninitialized(count);
    world_radii.SetNumUninitialized(count);
    for (int32 index{0}; index < count; ++index) {
        auto const column{index % 100};
        auto const row{(index / 100) % 100};
        FVector3f position{1000.0f,
                           FMath::Lerp(-800.0f, 800.0f, static_cast<float>(column) / 99.0f),
                           FMath::Lerp(-450.0f, 450.0f, static_cast<float>(row) / 99.0f)};
        if (visibility == EBenchmarkVisibility::Culled) {
            position.X = -1000.0f;
        } else if (visibility == EBenchmarkVisibility::Mixed) {
            if (index % 4 == 2) {
                position.Y = 1500.0f;
            } else if (index % 4 == 3) {
                position.X = -1000.0f;
            }
        }
        positions[index] = position;
        health[index] = static_cast<float>(index % 101) / 100.0f;
        world_radii[index] = 200.0f;
    }
    return {.positions = MoveTemp(positions),
            .health = MoveTemp(health),
            .world_radii = MoveTemp(world_radii)};
}

auto make_forward_x_projection() -> FMatrix44f {
    FMatrix44f projection{FMatrix44f::Identity};
    FMemory::Memzero(projection.M, sizeof(projection.M));
    projection.M[1][0] = 1.0f;
    projection.M[2][1] = 1.0f;
    projection.M[0][2] = 1.0f;
    projection.M[0][3] = 1.0f;
    return projection;
}

auto visibility_stage_name(EBenchmarkVisibility const visibility) -> FString {
    switch (visibility) {
        case EBenchmarkVisibility::Visible: {
            return TEXT("gpu_upload_raster_visible");
        }
        case EBenchmarkVisibility::Culled: {
            return TEXT("gpu_upload_raster_culled");
        }
        case EBenchmarkVisibility::Mixed: {
            return TEXT("gpu_upload_raster_mixed");
        }
        default: {
            checkNoEntry();
            return {};
        }
    }
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
        auto source{make_source(count, EBenchmarkVisibility::Visible)};
        TArray<FEntityOverlayInstance> collected;
        FEntityOverlayCollector collector;
        TArray<double> collection_samples;
        collection_samples.Reserve(measured_iterations);

        auto const total_iterations{warmup_iterations + measured_iterations};
        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            auto const start{FPlatformTime::Seconds()};
            collector.begin(FVector3f::ZeroVector, 100000.0f, collected);
            static_cast<void>(
                collector.append({source.positions, source.health, source.world_radii}));
            auto const elapsed{(FPlatformTime::Seconds() - start) * 1'000'000.0};
            if (iteration >= warmup_iterations) {
                collection_samples.Add(elapsed);
            }
        }

        auto frame_store{MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>()};
        auto& visible_frame{frame_store->next()};
        collector.begin(FVector3f::ZeroVector, 100000.0f, visible_frame.instances);
        static_cast<void>(collector.append({source.positions, source.health, source.world_radii}));
        frame_store->publish();

        TStrongObjectPtr<UTextureRenderTarget2D> output{NewObject<UTextureRenderTarget2D>()};
        check(output.IsValid());
        output->InitCustomFormat(1920, 1080, PF_R8G8B8A8, true);
        auto* const output_resource{output->GameThread_GetRenderTargetResource()};
        check(output_resource);
        FEntityOverlayView const view{.camera_origin = FVector3f::ZeroVector,
                                      .view_projection = make_forward_x_projection(),
                                      .view_rect = FIntRect{0, 0, 1920, 1080},
                                      .output_size = {1920, 1080}};
        FEntityOverlayStyle const style;
        FEntityOverlayRenderer renderer;
        TArray<double> submission_samples;
        TArray<double> gpu_samples;
        submission_samples.Reserve(measured_iterations);
        for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
            auto const start{FPlatformTime::Seconds()};
            renderer.render(frame_store, view, style, output_resource);
            auto const elapsed{(FPlatformTime::Seconds() - start) * 1'000'000.0};
            if (iteration >= warmup_iterations) {
                submission_samples.Add(elapsed);
            }
        }
        FlushRenderingCommands();

        report.results.Add(
            summarize(TEXT("collection_filter"), count, MoveTemp(collection_samples)));
        report.results.Add(
            summarize(TEXT("render_submission"), count, MoveTemp(submission_samples)));

        EBenchmarkVisibility const visibility_workloads[]{EBenchmarkVisibility::Visible,
                                                          EBenchmarkVisibility::Culled,
                                                          EBenchmarkVisibility::Mixed};
        for (auto const visibility : visibility_workloads) {
            auto workload_source{make_source(count, visibility)};
            auto& workload_frame{frame_store->next()};
            collector.begin(FVector3f::ZeroVector, 100000.0f, workload_frame.instances);
            static_cast<void>(collector.append(
                {workload_source.positions, workload_source.health, workload_source.world_radii}));
            frame_store->publish();

            gpu_samples.Reset();
            gpu_samples.Reserve(measured_iterations);
            for (int32 iteration{0}; iteration < total_iterations; ++iteration) {
                ENQUEUE_RENDER_COMMAND(MeasureEntityOverlayGPU)
                ([frame_store,
                  view,
                  style,
                  output_resource,
                  iteration,
                  warmup_iterations,
                  &gpu_samples](FRHICommandListImmediate& rhi_command_list) {
                    auto const measurement{measure_entity_overlay_gpu(
                        rhi_command_list, frame_store->current(), view, style, output_resource)};
                    if (iteration >= warmup_iterations && measurement.IsSet()) {
                        gpu_samples.Add(measurement.GetValue());
                    }
                });
            }
            FlushRenderingCommands();

            if (!gpu_samples.IsEmpty()) {
                report.results.Add(
                    summarize(visibility_stage_name(visibility), count, MoveTemp(gpu_samples)));
            }
        }
    }
    return report;
}

auto write_entity_overlay_debug_image(FEntityOverlayFrameStoreConstPtr frame_store,
                                      FEntityOverlayView const& view,
                                      FString const& output_path) -> bool {
    check(IsInGameThread());
    if (!frame_store.IsValid() || !view.is_valid()) {
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
    renderer.render(frame_store, view, FEntityOverlayStyle{}, output_resource);
    FlushRenderingCommands();
    renderer.render(frame_store, view, FEntityOverlayStyle{}, output_resource);
    FlushRenderingCommands();

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true);
    TUniquePtr<FArchive> archive{IFileManager::Get().CreateFileWriter(*output_path)};
    return archive.IsValid() && FImageUtils::ExportRenderTarget2DAsPNG(output.Get(), *archive);
}
