#include "HeatmapRDGRenderer.h"

#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeatmapRDG, Log, All);

namespace {
constexpr int32 thread_group_size{8};

class FHeatmapRDGCS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FHeatmapRDGCS);
    SHADER_USE_PARAMETER_STRUCT(FHeatmapRDGCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, GridSize)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, GridValues)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FHeatmapRDGCS,
                        "/Plugin/SandboxUI/Private/HeatmapRDG/HeatmapRDG.usf",
                        "render_heatmap_cs",
                        SF_Compute);

void execute_heatmap_graph(FRHICommandListImmediate& rhi_command_list,
                           TConstArrayView<float> const values,
                           FIntPoint const dimensions,
                           FTextureRHIRef const& output_texture_rhi) {
    FRDGBuilder graph_builder{rhi_command_list};
    auto const grid_buffer{CreateStructuredBuffer(graph_builder,
                                                  TEXT("HeatmapRDG.GridValues"),
                                                  sizeof(float),
                                                  values.Num(),
                                                  values.GetData(),
                                                  values.Num() * sizeof(float),
                                                  ERDGInitialDataFlags::NoCopy)};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("HeatmapRDG.Output")))};

    auto* const parameters{graph_builder.AllocParameters<FHeatmapRDGCS::FParameters>()};
    parameters->GridSize = dimensions;
    parameters->GridValues = graph_builder.CreateSRV(grid_buffer);
    parameters->OutputTexture = graph_builder.CreateUAV(output_texture);

    auto const shader{TShaderMapRef<FHeatmapRDGCS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const group_count{FComputeShaderUtils::GetGroupCount(dimensions, thread_group_size)};
    FComputeShaderUtils::AddPass(
        graph_builder,
        RDG_EVENT_NAME("HeatmapRDG.Render %dx%d", dimensions.X, dimensions.Y),
        shader,
        parameters,
        group_count);

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}
}

void render_heatmap_rdg(FRHICommandListImmediate& rhi_command_list,
                        TArray<float> values,
                        FIntPoint const dimensions,
                        FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());
    check(dimensions.X > 0 && dimensions.Y > 0);
    check(static_cast<int64>(dimensions.X) * static_cast<int64>(dimensions.Y) == values.Num());

    if (output_resource == nullptr) {
        UE_LOG(LogHeatmapRDG, Error, TEXT("Cannot render the heatmap without an output resource."));
        return;
    }

    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogHeatmapRDG, Error, TEXT("The heatmap output texture has no RHI resource."));
        return;
    }

    execute_heatmap_graph(rhi_command_list, values, dimensions, output_texture_rhi);
}

auto measure_heatmap_rdg_gpu(FRHICommandListImmediate& rhi_command_list,
                             TArray<float> values,
                             FIntPoint const dimensions,
                             FTextureRenderTargetResource* const output_resource)
    -> TOptional<double> {
    check(IsInRenderingThread());
    check(dimensions.X > 0 && dimensions.Y > 0);
    check(static_cast<int64>(dimensions.X) * static_cast<int64>(dimensions.Y) == values.Num());

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
    execute_heatmap_graph(rhi_command_list, values, dimensions, output_texture_rhi);
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
