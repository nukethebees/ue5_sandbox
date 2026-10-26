#include "SpaceEnergyFieldRenderer.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpaceEnergyFieldRenderer, Log, All);

namespace {
constexpr int32 thread_group_size{8};

class FSpaceEnergyFieldCS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FSpaceEnergyFieldCS);
    SHADER_USE_PARAMETER_STRUCT(FSpaceEnergyFieldCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(float, TimeSeconds)
    SHADER_PARAMETER(float, WarpScale)
    SHADER_PARAMETER(float, WarpStrength)
    SHADER_PARAMETER(float, StarDensity)
    SHADER_PARAMETER(float, StarIntensity)
    SHADER_PARAMETER(float, PlasmaIntensity)
    SHADER_PARAMETER(FVector4f, ColourA)
    SHADER_PARAMETER(FVector4f, ColourB)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FSpaceEnergyFieldCS,
                        "/Plugin/SandboxShaders/Private/SpaceEnergyField/SpaceEnergyField.usf",
                        "render_space_energy_field_cs",
                        SF_Compute);
}

void render_space_energy_field(FRHICommandListImmediate& rhi_command_list,
                               FSpaceEnergyFieldRenderParameters const& parameters,
                               FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());

    if (output_resource == nullptr) {
        UE_LOG(LogSpaceEnergyFieldRenderer, Error, TEXT("Space field output resource is null."));
        return;
    }

    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogSpaceEnergyFieldRenderer, Error, TEXT("Space field output has no RHI texture."));
        return;
    }

    FRDGBuilder graph_builder{rhi_command_list};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("SpaceEnergyField.Output")))};
    auto* const shader_parameters{
        graph_builder.AllocParameters<FSpaceEnergyFieldCS::FParameters>()};
    shader_parameters->OutputSize = parameters.output_size;
    shader_parameters->TimeSeconds = parameters.time_seconds;
    shader_parameters->WarpScale = parameters.warp_scale;
    shader_parameters->WarpStrength = parameters.warp_strength;
    shader_parameters->StarDensity = parameters.star_density;
    shader_parameters->StarIntensity = parameters.star_intensity;
    shader_parameters->PlasmaIntensity = parameters.plasma_intensity;
    shader_parameters->ColourA = parameters.colour_a;
    shader_parameters->ColourB = parameters.colour_b;
    shader_parameters->OutputTexture = graph_builder.CreateUAV(output_texture);

    auto const shader{TShaderMapRef<FSpaceEnergyFieldCS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const group_count{
        FComputeShaderUtils::GetGroupCount(parameters.output_size, thread_group_size)};
    FComputeShaderUtils::AddPass(graph_builder,
                                 RDG_EVENT_NAME("SpaceEnergyField.Render %dx%d",
                                                parameters.output_size.X,
                                                parameters.output_size.Y),
                                 shader,
                                 shader_parameters,
                                 group_count);

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}
