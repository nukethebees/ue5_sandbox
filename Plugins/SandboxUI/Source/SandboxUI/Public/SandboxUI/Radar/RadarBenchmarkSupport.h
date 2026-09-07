#pragma once

#include "SandboxUI/Radar/RadarFrameStore.h"

#include "Misc/Optional.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

SANDBOXUI_API void submit_radar_render(FRadarFrameStoreConstPtr frame_store,
                                       FRadarStyle const& style,
                                       FTextureRenderTargetResource* output_resource);

[[nodiscard]] SANDBOXUI_API auto measure_radar_gpu(FRHICommandListImmediate& command_list,
                                                   FRadarFrame const& frame,
                                                   FRadarStyle const& style,
                                                   FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
