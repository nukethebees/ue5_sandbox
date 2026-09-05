#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"

#include "Misc/Optional.h"

class FTextureRenderTargetResource;
class FRHICommandListImmediate;

class FEntityOverlayRenderer {
  public:
    void render(FEntityOverlayFrameStoreConstPtr frame_store,
                FEntityOverlayView const& view,
                FEntityOverlayStyle const& style,
                FTextureRenderTargetResource* output_resource) const;
};

[[nodiscard]] auto measure_entity_overlay_gpu(FRHICommandListImmediate& rhi_command_list,
                                              FEntityOverlayFrame const& frame,
                                              FEntityOverlayView const& view,
                                              FEntityOverlayStyle const& style,
                                              FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
