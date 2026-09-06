#include "Benchmarks/Radar3D/Radar3DBenchmark.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "RenderingThread.h"
#include "SandboxUI/Radar/RadarBenchmarkSupport.h"
#include "UObject/StrongObjectPtr.h"

void benchmark_radar_3d_rdg(FRadarFrame const& frame,
                            int32 const warmup_iterations,
                            int32 const measured_iterations,
                            TArray<double>& submission_samples,
                            TArray<double>& gpu_samples) {
    auto output_texture{
        TStrongObjectPtr<UTextureRenderTarget2D>{NewObject<UTextureRenderTarget2D>()}};
    check(output_texture.IsValid());
    output_texture->InitCustomFormat(512, 512, PF_R8G8B8A8, true);
    auto* const output_resource{output_texture->GameThread_GetRenderTargetResource()};
    check(output_resource);

    auto frame_store{MakeShared<FRadarFrameStore, ESPMode::ThreadSafe>()};
    frame_store->next() = frame;
    frame_store->publish();
    FRadarStyle const style;
    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        submit_radar_render(frame_store, style, output_resource);
    }
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        auto const start_seconds{FPlatformTime::Seconds()};
        submit_radar_render(frame_store, style, output_resource);
        submission_samples.Add((FPlatformTime::Seconds() - start_seconds) * 1'000'000.0);
    }

    // Synchronize outside the samples and keep the render target alive until every submission has
    // reached the render thread.
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        ENQUEUE_RENDER_COMMAND(WarmupRadar3DGPU)
        ([frame, style, output_resource](FRHICommandListImmediate& rhi_command_list) {
            (void)measure_radar_gpu(rhi_command_list, frame, style, output_resource);
        });
    }
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        ENQUEUE_RENDER_COMMAND(MeasureRadar3DGPU)
        ([frame, style, output_resource, &gpu_samples](FRHICommandListImmediate& rhi_command_list) {
            auto const measurement{
                measure_radar_gpu(rhi_command_list, frame, style, output_resource)};
            if (measurement.IsSet()) {
                gpu_samples.Add(measurement.GetValue());
            }
        });
    }
    FlushRenderingCommands();
}
