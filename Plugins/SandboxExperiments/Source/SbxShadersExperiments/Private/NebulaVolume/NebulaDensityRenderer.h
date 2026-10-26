#pragma once

#include "Math/IntVector.h"
#include "Math/Vector.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

struct FNebulaDensityRenderParameters {
    FIntVector output_size{FIntVector::ZeroValue};
    FVector3f feature_period{1.0f};
    FVector3f detail_period{1.0f};
    float seed{0.0f};
};

void render_nebula_density(FRHICommandListImmediate& rhi_command_list,
                           FNebulaDensityRenderParameters const& parameters,
                           FTextureRenderTargetResource* output_resource);
