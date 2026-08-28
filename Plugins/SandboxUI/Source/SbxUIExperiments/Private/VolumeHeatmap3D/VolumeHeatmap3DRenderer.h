#pragma once

#include "Containers/Array.h"
#include "Math/IntVector.h"
#include "Math/Vector4.h"
#include "Misc/Optional.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

struct FVolumeHeatmap3DGrid {
    FIntVector dimensions{64, 64, 64};
    TArray<float> values;

    [[nodiscard]] auto is_valid() const -> bool;
};

struct FVolumeHeatmap3DView {
    float yaw_degrees{-51.0f};
    float pitch_degrees{29.0f};
    float density_scale{3.0f};
    int32 slice_count{96};
};

class FVolumeHeatmap3DRenderer {
  public:
    void render(FVolumeHeatmap3DGrid const& grid,
                FVolumeHeatmap3DView view,
                FTextureRenderTargetResource* output_resource) const;
};

[[nodiscard]] auto measure_volume_heatmap_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                                                 FVolumeHeatmap3DGrid grid,
                                                 FVolumeHeatmap3DView view,
                                                 FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
