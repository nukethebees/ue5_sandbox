#include "Benchmarks/Heatmap/HeatmapBenchmark.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Experiments/HeatmapRDG/HeatmapRDGWidget.h"
#include "HAL/PlatformTime.h"
#include "HeatmapRDG/HeatmapRDGRenderer.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"

namespace {
auto make_rdg_grid(TConstArrayView<float> const values, int32 const resolution) -> FHeatmapRDGGrid {
    FHeatmapRDGGrid grid{.width = resolution, .height = resolution};
    grid.values.Append(values.GetData(), values.Num());
    return grid;
}
}

void benchmark_rdg_heatmap(TConstArrayView<float> const values,
                           int32 const resolution,
                           int32 const warmup_iterations,
                           int32 const measured_iterations,
                           TArray<double>& submission_samples,
                           TArray<double>& gpu_samples) {
    auto widget{TStrongObjectPtr<UHeatmapRDGWidget>{NewObject<UHeatmapRDGWidget>()}};
    check(widget.IsValid());
    auto slate_widget{widget->TakeWidget()};

    auto const grid{make_rdg_grid(values, resolution)};
    for (int32 iteration{0}; iteration < warmup_iterations; ++iteration) {
        check(widget->set_grid(grid));
    }
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        auto const start_seconds{FPlatformTime::Seconds()};
        check(widget->set_grid(grid));
        submission_samples.Add((FPlatformTime::Seconds() - start_seconds) * 1'000'000.0);
    }

    // This synchronization is outside the samples. It keeps captured grid snapshots and the
    // render-target resource alive until all benchmark submissions have reached the render thread.
    FlushRenderingCommands();

    auto output_texture{
        TStrongObjectPtr<UTextureRenderTarget2D>{NewObject<UTextureRenderTarget2D>()}};
    check(output_texture.IsValid());
    output_texture->bSupportsUAV = true;
    output_texture->InitCustomFormat(512, 512, PF_R8G8B8A8, true);
    auto* const output_resource{output_texture->GameThread_GetRenderTargetResource()};
    check(output_resource);
    auto const dimensions{FIntPoint{resolution, resolution}};

    TArray<float> warmup_values;
    warmup_values.Append(values.GetData(), values.Num());
    ENQUEUE_RENDER_COMMAND(WarmupHeatmapRDGGPU)
    ([snapshot = MoveTemp(warmup_values), dimensions, output_resource](
         FRHICommandListImmediate& rhi_command_list) mutable {
        render_heatmap_rdg(rhi_command_list, MoveTemp(snapshot), dimensions, output_resource);
    });
    FlushRenderingCommands();

    for (int32 iteration{0}; iteration < measured_iterations; ++iteration) {
        TArray<float> snapshot;
        snapshot.Append(values.GetData(), values.Num());
        ENQUEUE_RENDER_COMMAND(MeasureHeatmapRDGGPU)
        ([values_copy = MoveTemp(snapshot), dimensions, output_resource, &gpu_samples](
             FRHICommandListImmediate& rhi_command_list) mutable {
            auto const measurement{measure_heatmap_rdg_gpu(
                rhi_command_list, MoveTemp(values_copy), dimensions, output_resource)};
            if (measurement.IsSet()) {
                gpu_samples.Add(measurement.GetValue());
            }
        });
    }
    FlushRenderingCommands();
}
