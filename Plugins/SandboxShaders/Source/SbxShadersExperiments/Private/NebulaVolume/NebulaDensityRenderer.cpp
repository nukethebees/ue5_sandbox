#include "NebulaDensityRenderer.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogNebulaDensityRenderer, Log, All);

namespace {
constexpr int32 nebula_density_thread_group_size{4};

class FNebulaDensityCS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FNebulaDensityCS);
    SHADER_USE_PARAMETER_STRUCT(FNebulaDensityCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntVector, OutputSize)
    SHADER_PARAMETER(FVector3f, FeaturePeriod)
    SHADER_PARAMETER(FVector3f, DetailPeriod)
    SHADER_PARAMETER(float, Seed)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FNebulaDensityCS,
                        "/Plugin/SandboxShaders/Private/Nebula/NebulaDensity.usf",
                        "generate_nebula_density_cs",
                        SF_Compute);
}

void render_nebula_density(FRHICommandListImmediate& rhi_command_list,
                           FNebulaDensityRenderParameters const& parameters,
                           FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());

    if (output_resource == nullptr) {
        UE_LOG(LogNebulaDensityRenderer, Error, TEXT("Nebula density output resource is null."));
        return;
    }

    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogNebulaDensityRenderer,
               Error,
               TEXT("Nebula density output has no RHI texture."));
        return;
    }

    FRDGBuilder graph_builder{rhi_command_list};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("NebulaDensity.Output")))};
    auto* const shader_parameters{graph_builder.AllocParameters<FNebulaDensityCS::FParameters>()};
    shader_parameters->OutputSize = parameters.output_size;
    shader_parameters->FeaturePeriod = parameters.feature_period;
    shader_parameters->DetailPeriod = parameters.detail_period;
    shader_parameters->Seed = parameters.seed;
    shader_parameters->OutputTexture = graph_builder.CreateUAV(output_texture);

    auto const shader{TShaderMapRef<FNebulaDensityCS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const group_count{
        FComputeShaderUtils::GetGroupCount(parameters.output_size, nebula_density_thread_group_size)};
    FComputeShaderUtils::AddPass(graph_builder,
                                 RDG_EVENT_NAME("NebulaDensity.Generate %dx%dx%d",
                                                parameters.output_size.X,
                                                parameters.output_size.Y,
                                                parameters.output_size.Z),
                                 shader,
                                 shader_parameters,
                                 group_count);

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}
