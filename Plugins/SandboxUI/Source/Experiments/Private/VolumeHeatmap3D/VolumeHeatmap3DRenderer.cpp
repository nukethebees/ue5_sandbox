#include "VolumeHeatmap3DRenderer.h"

#include "CommonRenderResources.h"
#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolumeHeatmap3DRenderer, Log, All);

namespace ml::ui::volume_heatmap_3d {
constexpr int32 maximum_grid_dimension{128};
constexpr uint32 frame_line_count{15};

class FVolumeBackgroundVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeBackgroundVS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeBackgroundVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FVolumeBackgroundPS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeBackgroundPS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeBackgroundPS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FVolumeSliceVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeSliceVS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeSliceVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(FVector4f, ViewParameters)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FVolumeSlicePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeSlicePS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeSlicePS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntVector, GridSize)
    SHADER_PARAMETER(FVector4f, ViewParameters)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, VolumeValues)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FVolumeFrameVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeFrameVS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeFrameVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(FVector4f, ViewParameters)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FVolumeFramePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FVolumeFramePS);
    SHADER_USE_PARAMETER_STRUCT(FVolumeFramePS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

#define VOLUME_SHADER_PATH "/Plugin/SandboxUI/Private/VolumeHeatmap3D/VolumeHeatmap3D.usf"
IMPLEMENT_GLOBAL_SHADER(FVolumeBackgroundVS, VOLUME_SHADER_PATH, "volume_background_vs", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVolumeBackgroundPS, VOLUME_SHADER_PATH, "volume_background_ps", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVolumeSliceVS, VOLUME_SHADER_PATH, "volume_slice_vs", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVolumeSlicePS, VOLUME_SHADER_PATH, "volume_slice_ps", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVolumeFrameVS, VOLUME_SHADER_PATH, "volume_frame_vs", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVolumeFramePS, VOLUME_SHADER_PATH, "volume_frame_ps", SF_Pixel);
#undef VOLUME_SHADER_PATH

BEGIN_SHADER_PARAMETER_STRUCT(FVolumeBackgroundPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeBackgroundVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeBackgroundPS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVolumeSlicePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeSliceVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeSlicePS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVolumeFramePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeFrameVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void set_pipeline(FRHICommandList& rhi_command_list,
                  FRHIVertexShader* const vertex_shader,
                  FRHIPixelShader* const pixel_shader,
                  bool const translucent) {
    FGraphicsPipelineStateInitializer pipeline_state;
    rhi_command_list.ApplyCachedRenderTargets(pipeline_state);
    pipeline_state.BlendState = translucent ? TStaticBlendState<CW_RGBA,
                                                                BO_Add,
                                                                BF_SourceAlpha,
                                                                BF_InverseSourceAlpha,
                                                                BO_Add,
                                                                BF_One,
                                                                BF_InverseSourceAlpha>::GetRHI()
                                            : TStaticBlendState<>::GetRHI();
    pipeline_state.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
    pipeline_state.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    pipeline_state.BoundShaderState.VertexDeclarationRHI =
        GEmptyVertexDeclaration.VertexDeclarationRHI;
    pipeline_state.BoundShaderState.VertexShaderRHI = vertex_shader;
    pipeline_state.BoundShaderState.PixelShaderRHI = pixel_shader;
    pipeline_state.PrimitiveType = PT_TriangleList;
    SetGraphicsPipelineState(rhi_command_list, pipeline_state, 0);
}

void set_viewport(FRHICommandList& rhi_command_list, FIntPoint const output_size) {
    rhi_command_list.SetViewport(0.0f, 0.0f, 0.0f, output_size.X, output_size.Y, 1.0f);
    rhi_command_list.SetStreamSource(0, nullptr, 0);
}

auto make_view_parameters(FVolumeHeatmap3DView const view) -> FVector4f {
    return FVector4f{FMath::DegreesToRadians(view.yaw_degrees),
                     FMath::DegreesToRadians(view.pitch_degrees),
                     view.density_scale,
                     static_cast<float>(view.slice_count)};
}

void add_background_pass(FRDGBuilder& graph_builder,
                         FRDGTextureRef const output_texture,
                         FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FVolumeBackgroundPassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->PS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ENoAction};
    auto const vertex_shader{
        TShaderMapRef<FVolumeBackgroundVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FVolumeBackgroundPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("VolumeHeatmap3D.Background"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_pipeline(rhi_command_list,
                         vertex_shader.GetVertexShader(),
                         pixel_shader.GetPixelShader(),
                         false);
            set_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            SetShaderParameters(
                rhi_command_list, pixel_shader, pixel_shader.GetPixelShader(), parameters->PS);
            rhi_command_list.DrawPrimitive(0, 2, 1);
        });
}

void add_slice_pass(FRDGBuilder& graph_builder,
                    FRDGTextureRef const output_texture,
                    FRDGBufferSRVRef const volume_values,
                    FIntVector const grid_size,
                    FVolumeHeatmap3DView const view,
                    FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FVolumeSlicePassParameters>()};
    auto const view_parameters{make_view_parameters(view)};
    parameters->VS.OutputSize = output_size;
    parameters->VS.ViewParameters = view_parameters;
    parameters->PS.GridSize = grid_size;
    parameters->PS.ViewParameters = view_parameters;
    parameters->PS.VolumeValues = volume_values;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};
    auto const vertex_shader{
        TShaderMapRef<FVolumeSliceVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FVolumeSlicePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("VolumeHeatmap3D.Slices Grid=%dx%dx%d Slices=%d",
                       grid_size.X,
                       grid_size.Y,
                       grid_size.Z,
                       view.slice_count),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size, slice_count = view.slice_count](
            FRDGAsyncTask, FRHICommandList& rhi_command_list) {
            set_pipeline(rhi_command_list,
                         vertex_shader.GetVertexShader(),
                         pixel_shader.GetPixelShader(),
                         true);
            set_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            SetShaderParameters(
                rhi_command_list, pixel_shader, pixel_shader.GetPixelShader(), parameters->PS);
            rhi_command_list.DrawPrimitive(0, 2, slice_count);
        });
}

void add_frame_pass(FRDGBuilder& graph_builder,
                    FRDGTextureRef const output_texture,
                    FVolumeHeatmap3DView const view,
                    FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FVolumeFramePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->VS.ViewParameters = make_view_parameters(view);
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};
    auto const vertex_shader{
        TShaderMapRef<FVolumeFrameVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FVolumeFramePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("VolumeHeatmap3D.Frame"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_pipeline(rhi_command_list,
                         vertex_shader.GetVertexShader(),
                         pixel_shader.GetPixelShader(),
                         true);
            set_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            rhi_command_list.DrawPrimitive(0, 2, frame_line_count);
        });
}

void execute_graph(FRHICommandListImmediate& rhi_command_list,
                   FVolumeHeatmap3DGrid const& grid,
                   FVolumeHeatmap3DView const view,
                   FTextureRHIRef const& output_texture_rhi) {
    auto const output_size{FIntPoint{static_cast<int32>(output_texture_rhi->GetSizeX()),
                                     static_cast<int32>(output_texture_rhi->GetSizeY())}};
    FRDGBuilder graph_builder{rhi_command_list};
    auto const values_buffer{CreateStructuredBuffer(graph_builder,
                                                    TEXT("VolumeHeatmap3D.Values"),
                                                    sizeof(float),
                                                    grid.values.Num(),
                                                    grid.values.GetData(),
                                                    grid.values.Num() * sizeof(float),
                                                    ERDGInitialDataFlags::NoCopy)};
    auto const values_srv{graph_builder.CreateSRV(values_buffer)};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("VolumeHeatmap3D.Output")))};
    add_background_pass(graph_builder, output_texture, output_size);
    add_slice_pass(graph_builder, output_texture, values_srv, grid.dimensions, view, output_size);
    add_frame_pass(graph_builder, output_texture, view, output_size);
    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}

void render_on_render_thread(FRHICommandListImmediate& rhi_command_list,
                             FVolumeHeatmap3DGrid const& grid,
                             FVolumeHeatmap3DView const view,
                             FTextureRenderTargetResource* const output_resource) {
    check(IsInRenderingThread());
    if (output_resource == nullptr) {
        UE_LOG(
            LogVolumeHeatmap3DRenderer, Error, TEXT("The volume heatmap has no output resource."));
        return;
    }
    auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_texture_rhi.IsValid()) {
        UE_LOG(LogVolumeHeatmap3DRenderer,
               Error,
               TEXT("The volume heatmap output has no RHI texture."));
        return;
    }
    execute_graph(rhi_command_list, grid, view, output_texture_rhi);
}
} // namespace

auto FVolumeHeatmap3DGrid::is_valid() const -> bool {
    if (dimensions.X <= 0 || dimensions.Y <= 0 || dimensions.Z <= 0 ||
        dimensions.X > ml::ui::volume_heatmap_3d::maximum_grid_dimension ||
        dimensions.Y > ml::ui::volume_heatmap_3d::maximum_grid_dimension ||
        dimensions.Z > ml::ui::volume_heatmap_3d::maximum_grid_dimension) {
        return false;
    }
    int64 const voxel_count{static_cast<int64>(dimensions.X) * dimensions.Y * dimensions.Z};
    return voxel_count <= MAX_int32 && values.Num() == voxel_count;
}

void FVolumeHeatmap3DRenderer::render(FVolumeHeatmap3DGrid const& grid,
                                      FVolumeHeatmap3DView const view,
                                      FTextureRenderTargetResource* const output_resource) const {
    check(IsInGameThread());
    if (!grid.is_valid() || view.slice_count <= 0 || view.slice_count > 256 ||
        view.density_scale <= 0.0f) {
        UE_LOG(
            LogVolumeHeatmap3DRenderer, Warning, TEXT("Skipped an invalid volume heatmap render."));
        return;
    }
    if (output_resource == nullptr) {
        UE_LOG(LogVolumeHeatmap3DRenderer,
               Error,
               TEXT("Cannot enqueue a volume heatmap without an output resource."));
        return;
    }
    FVolumeHeatmap3DGrid grid_snapshot{grid};
    ENQUEUE_RENDER_COMMAND(RenderVolumeHeatmap3D)
    ([grid = MoveTemp(grid_snapshot), view, output_resource](
         FRHICommandListImmediate& rhi_command_list) {
        ml::ui::volume_heatmap_3d::render_on_render_thread(
            rhi_command_list, grid, view, output_resource);
    });
}

auto measure_volume_heatmap_3d_gpu(FRHICommandListImmediate& rhi_command_list,
                                   FVolumeHeatmap3DGrid grid,
                                   FVolumeHeatmap3DView const view,
                                   FTextureRenderTargetResource* const output_resource)
    -> TOptional<double> {
    check(IsInRenderingThread());
    check(grid.is_valid());
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
    ml::ui::volume_heatmap_3d::execute_graph(rhi_command_list, grid, view, output_texture_rhi);
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
