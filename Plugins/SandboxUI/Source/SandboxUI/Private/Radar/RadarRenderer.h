#pragma once

#include "SandboxUI/Radar/RadarFrameStore.h"

class FTextureRenderTargetResource;

class FRadarRenderer {
  public:
    void render(FRadarFrameStoreConstPtr frame_store,
                FRadarStyle const& style,
                FTextureRenderTargetResource* output_resource) const;
};
