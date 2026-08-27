#include "Benchmarks/Radar3D/Radar3DBenchmark.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "Radar3D/Radar3DRenderer.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"

void benchmark_radar_3d_rdg(TConstArrayView<FRadar3DContact> const contacts,
                            int32 const warmup_iterations,
                            int32 const measured_iterations,
                            TArray<double>& submission_samples,
                            TArray<double>& gpu_samples) {
    auto output_texture{
        TStrongObjectPtr<UTextureRenderTarget2D>{NewObject<UTextureRenderTarget2D>()}};
    check(output_texture.IsValid());
    output_texture->bSupportsUAV = true;
    output_texture->InitCustomFormat(512, 512, PF_R8G8B8A8, true);
    auto* const output_resource{output_texture->GameThread_GetRenderTargetResource()};
    check(output_resource);

    FRadar3DRenderer renderer;
    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        renderer.render(contacts, output_resource);
    }
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        auto const start_seconds{FPlatformTime::Seconds()};
        renderer.render(contacts, output_resource);
        submission_samples.Add((FPlatformTime::Seconds() - start_seconds) * 1'000'000.0);
    }

    // Synchronize outside the samples and keep the render target alive until every submission has
    // reached the render thread.
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        TArray<FRadar3DContact> snapshot{contacts};
        ENQUEUE_RENDER_COMMAND(WarmupRadar3DGPU)
        ([contacts_copy = MoveTemp(snapshot),
          output_resource](FRHICommandListImmediate& rhi_command_list) mutable {
            (void)measure_radar_3d_gpu(rhi_command_list, MoveTemp(contacts_copy), output_resource);
        });
    }
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        TArray<FRadar3DContact> snapshot{contacts};
        ENQUEUE_RENDER_COMMAND(MeasureRadar3DGPU)
        ([contacts_copy = MoveTemp(snapshot), output_resource, &gpu_samples](
             FRHICommandListImmediate& rhi_command_list) mutable {
            auto const measurement{
                measure_radar_3d_gpu(rhi_command_list, MoveTemp(contacts_copy), output_resource)};
            if (measurement.IsSet()) {
                gpu_samples.Add(measurement.GetValue());
            }
        });
    }
    FlushRenderingCommands();
}
