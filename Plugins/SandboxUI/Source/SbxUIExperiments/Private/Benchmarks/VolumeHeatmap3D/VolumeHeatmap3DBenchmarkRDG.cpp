#include "Benchmarks/VolumeHeatmap3D/VolumeHeatmap3DBenchmark.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "VolumeHeatmap3D/VolumeHeatmap3DRenderer.h"

void benchmark_volume_heatmap_3d_rdg(FVolumeHeatmap3DGrid const& grid,
                                     FVolumeHeatmap3DView const view,
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
    FVolumeHeatmap3DRenderer renderer;
    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        renderer.render(grid, view, output_resource);
    }
    FlushRenderingCommands();
    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        auto const start_seconds{FPlatformTime::Seconds()};
        renderer.render(grid, view, output_resource);
        submission_samples.Add((FPlatformTime::Seconds() - start_seconds) * 1'000'000.0);
    }
    FlushRenderingCommands();
    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        FVolumeHeatmap3DGrid snapshot{grid};
        ENQUEUE_RENDER_COMMAND(WarmupVolumeHeatmap3DGPU)
        ([grid_copy = MoveTemp(snapshot), view, output_resource](
             FRHICommandListImmediate& rhi_command_list) mutable {
            (void)measure_volume_heatmap_3d_gpu(
                rhi_command_list, MoveTemp(grid_copy), view, output_resource);
        });
    }
    FlushRenderingCommands();
    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        FVolumeHeatmap3DGrid snapshot{grid};
        ENQUEUE_RENDER_COMMAND(MeasureVolumeHeatmap3DGPU)
        ([grid_copy = MoveTemp(snapshot), view, output_resource, &gpu_samples](
             FRHICommandListImmediate& rhi_command_list) mutable {
            auto const measurement{measure_volume_heatmap_3d_gpu(
                rhi_command_list, MoveTemp(grid_copy), view, output_resource)};
            if (measurement.IsSet()) {
                gpu_samples.Add(measurement.GetValue());
            }
        });
    }
    FlushRenderingCommands();
}
