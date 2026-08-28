#pragma once

#include "Containers/ArrayView.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Misc/Optional.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

struct FRadar3DContact {
    FVector3f position{FVector3f::ZeroVector};
    float size{5.0f};
    FVector4f color{1.0f, 1.0f, 1.0f, 1.0f};
};

class FRadar3DRenderer {
  public:
    void render(TConstArrayView<FRadar3DContact> contacts,
                FTextureRenderTargetResource* output_resource) const;
};

[[nodiscard]] auto measure_radar_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                                        TArray<FRadar3DContact> contacts,
                                        FTextureRenderTargetResource* output_resource)
    -> TOptional<double>;
