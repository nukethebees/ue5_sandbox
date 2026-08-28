#include "Scatter3DRenderer.h"

#include "CommonRenderResources.h"
#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogScatter3DRenderer, Log, All);

namespace ml::ui::scatter_3d {
constexpr uint32 frame_line_count{33};

static_assert(sizeof(FScatter3DPoint) == sizeof(float) * 8,
              "FScatter3DPoint must match ScatterSample in Scatter3D.usf.");

class FScatterBackgroundVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterBackgroundVS);
    SHADER_USE_PARAMETER_STRUCT(FScatterBackgroundVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FScatterBackgroundPS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterBackgroundPS);
    SHADER_USE_PARAMETER_STRUCT(FScatterBackgroundPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FScatterFrameVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterFrameVS);
    SHADER_USE_PARAMETER_STRUCT(FScatterFrameVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FScatterFramePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterFramePS);
    SHADER_USE_PARAMETER_STRUCT(FScatterFramePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FScatterSampleVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterSampleVS);
    SHADER_USE_PARAMETER_STRUCT(FScatterSampleVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScatter3DPoint>, Samples)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FScatterSamplePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FScatterSamplePS);
    SHADER_USE_PARAMETER_STRUCT(FScatterSamplePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FScatterBackgroundVS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_background_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FScatterBackgroundPS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_background_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FScatterFrameVS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_frame_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FScatterFramePS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_frame_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FScatterSampleVS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_sample_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FScatterSamplePS,
                        "/Plugin/SandboxUI/Private/Scatter3D/Scatter3D.usf",
                        "scatter_sample_ps",
                        SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FScatterBackgroundPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FScatterBackgroundVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FScatterBackgroundPS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FScatterFramePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FScatterFrameVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FScatterSamplePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FScatterSampleVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void set_scatter_pipeline(FRHICommandList& rhi_command_list,
                          FRHIVertexShader* const vertex_shader,
                          FRHIPixelShader* const pixel_shader,
                          bool const depth_test) {
    FGraphicsPipelineStateInitializer pipeline_state;
    rhi_command_list.ApplyCachedRenderTargets(pipeline_state);
    pipeline_state.BlendState = TStaticBlendState<CW_RGBA,
                                                  BO_Add,
                                                  BF_SourceAlpha,
                                                  BF_InverseSourceAlpha,
                                                  BO_Add,
                                                  BF_One,
                                                  BF_InverseSourceAlpha>::GetRHI();
    pipeline_state.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
    pipeline_state.DepthStencilState = depth_test
                                         ? TStaticDepthStencilState<true, CF_GreaterEqual>::GetRHI()
                                         : TStaticDepthStencilState<false, CF_Always>::GetRHI();
    pipeline_state.BoundShaderState.VertexDeclarationRHI =
        GEmptyVertexDeclaration.VertexDeclarationRHI;
    pipeline_state.BoundShaderState.VertexShaderRHI = vertex_shader;
    pipeline_state.BoundShaderState.PixelShaderRHI = pixel_shader;
    pipeline_state.PrimitiveType = PT_TriangleList;
    SetGraphicsPipelineState(rhi_command_list, pipeline_state, 0);
}

void set_scatter_viewport(FRHICommandList& rhi_command_list, FIntPoint const output_size) {
    rhi_command_list.SetViewport(0.0f, 0.0f, 0.0f, output_size.X, output_size.Y, 1.0f);
    rhi_command_list.SetStreamSource(0, nullptr, 0);
}

void add_scatter_background_pass(FRDGBuilder& graph_builder,
                                 FRDGTextureRef const output_texture,
                                 FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FScatterBackgroundPassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->PS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ENoAction};

    auto const vertex_shader{
        TShaderMapRef<FScatterBackgroundVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FScatterBackgroundPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("Scatter3D.Background"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_scatter_pipeline(rhi_command_list,
                                 vertex_shader.GetVertexShader(),
                                 pixel_shader.GetPixelShader(),
                                 false);
            set_scatter_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            SetShaderParameters(
                rhi_command_list, pixel_shader, pixel_shader.GetPixelShader(), parameters->PS);
            rhi_command_list.DrawPrimitive(0, 2, 1);
        });
}

void add_frame_pass(FRDGBuilder& graph_builder,
                    FRDGTextureRef const output_texture,
                    FRDGTextureRef const depth_texture,
                    FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FScatterFramePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};
    parameters->RenderTargets.DepthStencil =
        FDepthStencilBinding{depth_texture,
                             ERenderTargetLoadAction::EClear,
                             ERenderTargetLoadAction::ENoAction,
                             FExclusiveDepthStencil::DepthWrite_StencilNop};

    auto const vertex_shader{
        TShaderMapRef<FScatterFrameVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FScatterFramePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("Scatter3D.Frame"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_scatter_pipeline(rhi_command_list,
                                 vertex_shader.GetVertexShader(),
                                 pixel_shader.GetPixelShader(),
                                 true);
            set_scatter_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            rhi_command_list.DrawPrimitive(0, 2, frame_line_count);
        });
}

void add_sample_pass(FRDGBuilder& graph_builder,
                     FRDGTextureRef const output_texture,
                     FRDGTextureRef const depth_texture,
                     FRDGBufferSRVRef const samples,
                     int32 const point_count,
                     FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FScatterSamplePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->VS.Samples = samples;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};
    parameters->RenderTargets.DepthStencil =
        FDepthStencilBinding{depth_texture,
                             ERenderTargetLoadAction::ELoad,
                             ERenderTargetLoadAction::ENoAction,
                             FExclusiveDepthStencil::DepthWrite_StencilNop};

    auto const vertex_shader{
        TShaderMapRef<FScatterSampleVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FScatterSamplePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(RDG_EVENT_NAME("Scatter3D.Samples Points=%d", point_count),
                          parameters,
                          ERDGPassFlags::Raster,
                          [parameters, vertex_shader, pixel_shader, output_size, point_count](
                              FRDGAsyncTask, FRHICommandList& rhi_command_list) {
                              set_scatter_pipeline(rhi_command_list,
                                                   vertex_shader.GetVertexShader(),
                                                   pixel_shader.GetPixelShader(),
                                                   true);
                              set_scatter_viewport(rhi_command_list, output_size);
                              SetShaderParameters(rhi_command_list,
                                                  vertex_shader,
                                                  vertex_shader.GetVertexShader(),
                                                  parameters->VS);
                              rhi_command_list.DrawPrimitive(0, 2, point_count);
                          });
}

void execute_scatter_graph(FRHICommandListImmediate& rhi_command_list,
                           TConstArrayView<FScatter3DPoint> const points,
                           FTextureRHIRef const& output_texture_rhi) {
    auto const output_size{FIntPoint{static_cast<int32>(output_texture_rhi->GetSizeX()),
                                     static_cast<int32>(output_texture_rhi->GetSizeY())}};
    FRDGBuilder graph_builder{rhi_command_list};
    auto const sample_buffer{CreateStructuredBuffer(graph_builder,
                                                    TEXT("Scatter3D.Samples"),
                                                    sizeof(FScatter3DPoint),
                                                    points.Num(),
                                                    points.GetData(),
                                                    points.Num() * sizeof(FScatter3DPoint),
                                                    ERDGInitialDataFlags::NoCopy)};
    auto const sample_srv{graph_builder.CreateSRV(sample_buffer)};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("Scatter3D.Output")))};
    auto const depth_texture{
        graph_builder.CreateTexture(FRDGTextureDesc::Create2D(output_size,
                                                              PF_DepthStencil,
                                                              FClearValueBinding::DepthFar,
                                                              TexCreate_DepthStencilTargetable),
                                    TEXT("Scatter3D.Depth"))};

    add_scatter_background_pass(graph_builder, output_texture, output_size);
    add_frame_pass(graph_builder, output_texture, depth_texture, output_size);
    add_sample_pass(
        graph_builder, output_texture, depth_texture, sample_srv, points.Num(), output_size);

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}

void render_scatter_on_render_thread(FRHICommandListImmediate& rhi_command_list,
                                     TArray<FScatter3DPoint> const& points,
                                     FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());

    if (output_resource == nullptr) {
        UE_LOG(LogScatter3DRenderer,
               Error,
               TEXT("Cannot render the 3D scatter plot without an output resource."));
        return;
    }

    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogScatter3DRenderer, Error, TEXT("The 3D scatter output has no RHI texture."));
        return;
    }

    execute_scatter_graph(rhi_command_list, points, output_texture_rhi);
}
} // namespace

void FScatter3DRenderer::render(TConstArrayView<FScatter3DPoint> const points,
                                FTextureRenderTargetResource* const output_resource) const {
    check(IsInGameThread());

    if (points.IsEmpty()) {
        UE_LOG(LogScatter3DRenderer, Warning, TEXT("Skipped 3D scatter render with no points."));
        return;
    }
    if (output_resource == nullptr) {
        UE_LOG(LogScatter3DRenderer,
               Error,
               TEXT("Cannot enqueue the 3D scatter render without an output resource."));
        return;
    }

    TArray<FScatter3DPoint> points_snapshot{points};
    ENQUEUE_RENDER_COMMAND(RenderScatter3D)
    ([points = MoveTemp(points_snapshot),
      output_resource](FRHICommandListImmediate& rhi_command_list) {
        ml::ui::scatter_3d::render_scatter_on_render_thread(
            rhi_command_list, points, output_resource);
    });
}

auto measure_scatter_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                            TArray<FScatter3DPoint> points,
                            FTextureRenderTargetResource* const output_resource)
    -> TOptional<double> {
    check(IsInRenderingThread());
    check(!points.IsEmpty());

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
    ml::ui::scatter_3d::execute_scatter_graph(rhi_command_list, points, output_texture_rhi);
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
