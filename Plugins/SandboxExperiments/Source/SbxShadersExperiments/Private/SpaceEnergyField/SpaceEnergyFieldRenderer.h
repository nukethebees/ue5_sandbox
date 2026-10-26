#pragma once

#include "Math/IntPoint.h"
#include "Math/Vector4.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;

struct FSpaceEnergyFieldRenderParameters {
    FIntPoint output_size{FIntPoint::ZeroValue};
    float time_seconds{0.0f};
    float warp_scale{1.0f};
    float warp_strength{0.0f};
    float star_density{0.0f};
    float star_intensity{0.0f};
    float plasma_intensity{0.0f};
    FVector4f colour_a{0.0f};
    FVector4f colour_b{0.0f};
};

void render_space_energy_field(FRHICommandListImmediate& rhi_command_list,
                               FSpaceEnergyFieldRenderParameters const& parameters,
                               FTextureRenderTargetResource* output_resource);
