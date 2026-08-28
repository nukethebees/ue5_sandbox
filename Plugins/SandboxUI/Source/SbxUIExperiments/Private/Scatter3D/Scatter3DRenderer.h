#pragma once

#include "Containers/ArrayView.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Misc/Optional.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

struct FScatter3DPoint {
    FVector3f position{FVector3f::ZeroVector};
    float size{3.0f};
    FVector4f color{1.0f, 1.0f, 1.0f, 1.0f};
};

class FScatter3DRenderer {
  public:
    void render(TConstArrayView<FScatter3DPoint> points,
                FTextureRenderTargetResource* output_resource) const;
};

[[nodiscard]] auto measure_scatter_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                                          TArray<FScatter3DPoint> points,
                                          FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
