#pragma once

#include "Containers/Array.h"
#include "Math/IntPoint.h"
#include "Misc/Optional.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

void render_heatmap_rdg(FRHICommandListImmediate& rhi_command_list,
                        TArray<float> values,
                        FIntPoint dimensions,
                        FTextureRenderTargetResource* output_resource);

[[nodiscard]] auto measure_heatmap_rdg_gpu(FRHICommandListImmediate& rhi_command_list,
                                           TArray<float> values,
                                           FIntPoint dimensions,
                                           FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
