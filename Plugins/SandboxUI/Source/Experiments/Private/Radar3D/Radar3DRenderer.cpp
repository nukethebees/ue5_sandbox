#include "Radar3DRenderer.h"

#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadar3DRenderer, Log, All);

namespace {
constexpr int32 thread_group_size{8};

static_assert(sizeof(FRadar3DContact) == sizeof(float) * 8,
              "FRadar3DContact must match RadarContact in Radar3D.usf.");

class FRadar3DCS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadar3DCS);
    SHADER_USE_PARAMETER_STRUCT(FRadar3DCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(uint32, ContactCount)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRadar3DContact>, Contacts)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FRadar3DCS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "render_radar_cs",
                        SF_Compute);

void execute_radar_graph(FRHICommandListImmediate& rhi_command_list,
                         TConstArrayView<FRadar3DContact> const contacts,
                         FTextureRHIRef const& output_texture_rhi) {
    auto const output_size{FIntPoint{static_cast<int32>(output_texture_rhi->GetSizeX()),
                                     static_cast<int32>(output_texture_rhi->GetSizeY())}};
    FRDGBuilder graph_builder{rhi_command_list};
    auto const contact_buffer{CreateStructuredBuffer(graph_builder,
                                                     TEXT("Radar3D.Contacts"),
                                                     sizeof(FRadar3DContact),
                                                     contacts.Num(),
                                                     contacts.GetData(),
                                                     contacts.Num() * sizeof(FRadar3DContact),
                                                     ERDGInitialDataFlags::NoCopy)};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("Radar3D.Output")))};

    auto* const parameters{graph_builder.AllocParameters<FRadar3DCS::FParameters>()};
    parameters->OutputSize = output_size;
    parameters->ContactCount = static_cast<uint32>(contacts.Num());
    parameters->Contacts = graph_builder.CreateSRV(contact_buffer);
    parameters->OutputTexture = graph_builder.CreateUAV(output_texture);

    auto const shader{TShaderMapRef<FRadar3DCS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const group_count{FComputeShaderUtils::GetGroupCount(output_size, thread_group_size)};
    FComputeShaderUtils::AddPass(graph_builder,
                                 RDG_EVENT_NAME("Radar3D.Render Contacts=%d", contacts.Num()),
                                 shader,
                                 parameters,
                                 group_count);

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}

void render_radar_on_render_thread(FRHICommandListImmediate& rhi_command_list,
                                   TArray<FRadar3DContact> const& contacts,
                                   FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());

    if (output_resource == nullptr) {
        UE_LOG(LogRadar3DRenderer,
               Error,
               TEXT("Cannot render the 3D radar without an output resource."));
        return;
    }

    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogRadar3DRenderer, Error, TEXT("The 3D radar output has no RHI texture."));
        return;
    }

    execute_radar_graph(rhi_command_list, contacts, output_texture_rhi);
}
} // namespace

void FRadar3DRenderer::render(TConstArrayView<FRadar3DContact> const contacts,
                              FTextureRenderTargetResource* const output_resource) const {
    check(IsInGameThread());

    if (contacts.IsEmpty()) {
        UE_LOG(LogRadar3DRenderer, Warning, TEXT("Skipped 3D radar render with no contacts."));
        return;
    }
    if (output_resource == nullptr) {
        UE_LOG(LogRadar3DRenderer,
               Error,
               TEXT("Cannot enqueue the 3D radar render without an output resource."));
        return;
    }

    TArray<FRadar3DContact> contacts_snapshot{contacts};
    ENQUEUE_RENDER_COMMAND(RenderRadar3D)
    ([contacts = MoveTemp(contacts_snapshot),
      output_resource](FRHICommandListImmediate& rhi_command_list) {
        render_radar_on_render_thread(rhi_command_list, contacts, output_resource);
    });
}

auto measure_radar_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                          TArray<FRadar3DContact> contacts,
                          FTextureRenderTargetResource* const output_resource)
    -> TOptional<double> {
    check(IsInRenderingThread());
    check(!contacts.IsEmpty());

    if (!GSupportsTimestampRenderQueries || output_resource == nullptr) {
        return {};
    }
    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        return {};
    }

    auto const query_pool{RHICreateRenderQueryPool(RQT_AbsoluteTime, 2)};
    auto start_query{query_pool->AllocateQuery()};
    auto end_query{query_pool->AllocateQuery()};
    rhi_command_list.EndRenderQuery(start_query.GetQuery());
    execute_radar_graph(rhi_command_list, contacts, output_texture_rhi);
    rhi_command_list.EndRenderQuery(end_query.GetQuery());
    rhi_command_list.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

    uint64 start_microseconds{0};
    uint64 end_microseconds{0};
    if (!RHIGetRenderQueryResult(start_query.GetQuery(), start_microseconds, true) ||
        !RHIGetRenderQueryResult(end_query.GetQuery(), end_microseconds, true) ||
        end_microseconds < start_microseconds) {
        return {};
    }
    return static_cast<double>(end_microseconds - start_microseconds);
}
