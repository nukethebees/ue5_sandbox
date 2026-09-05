#include "EntityOverlayRenderer.h"

#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogEntityOverlayRenderer, Log, All);

namespace ml::ui::entity_overlay {
static_assert(sizeof(FEntityOverlayInstance) == sizeof(float) * 4,
              "FEntityOverlayInstance must match EntityOverlayInstance in EntityOverlay.usf.");

class FEntityOverlayVS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FEntityOverlayVS);
    SHADER_USE_PARAMETER_STRUCT(FEntityOverlayVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FMatrix44f, ViewProjection)
    SHADER_PARAMETER(FVector3f, CameraOrigin)
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(FIntVector4, ViewRect)
    SHADER_PARAMETER(FVector2f, BarSizePixels)
    SHADER_PARAMETER(FVector2f, ScreenOffsetPixels)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FEntityOverlayInstance>, Instances)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FEntityOverlayPS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FEntityOverlayPS);
    SHADER_USE_PARAMETER_STRUCT(FEntityOverlayPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FVector2f, BarSizePixels)
    SHADER_PARAMETER(float, InsetPixels)
    SHADER_PARAMETER(FVector4f, BackgroundColor)
    SHADER_PARAMETER(FVector4f, FillColor)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FEntityOverlayVS,
                        "/Plugin/SandboxUI/Private/EntityOverlay/EntityOverlay.usf",
                        "entity_overlay_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FEntityOverlayPS,
                        "/Plugin/SandboxUI/Private/EntityOverlay/EntityOverlay.usf",
                        "entity_overlay_ps",
                        SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FEntityOverlayPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FEntityOverlayVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FEntityOverlayPS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void execute_graph(FRHICommandListImmediate& rhi_command_list,
                   FEntityOverlayFrame const& frame,
                   FEntityOverlayView const& view,
                   FEntityOverlayStyle const& style,
                   FTextureRHIRef const& output_texture_rhi) {
    FRDGBuilder graph_builder{rhi_command_list};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("EntityOverlay.Output")))};

    if (frame.instances.IsEmpty()) {
        AddClearRenderTargetPass(graph_builder, output_texture, FLinearColor::Transparent);
        graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
        graph_builder.Execute();
        return;
    }

    auto const instance_buffer{
        CreateStructuredBuffer(graph_builder,
                               TEXT("EntityOverlay.Instances"),
                               sizeof(FEntityOverlayInstance),
                               frame.instances.Num(),
                               frame.instances.GetData(),
                               frame.instances.Num() * sizeof(FEntityOverlayInstance),
                               ERDGInitialDataFlags::NoCopy)};
    auto* const parameters{graph_builder.AllocParameters<FEntityOverlayPassParameters>()};
    parameters->VS.ViewProjection = view.view_projection;
    parameters->VS.CameraOrigin = view.camera_origin;
    parameters->VS.OutputSize = view.output_size;
    parameters->VS.ViewRect = {
        view.view_rect.Min.X, view.view_rect.Min.Y, view.view_rect.Max.X, view.view_rect.Max.Y};
    parameters->VS.BarSizePixels = style.bar_size_pixels;
    parameters->VS.ScreenOffsetPixels = style.screen_offset_pixels;
    parameters->VS.Instances = graph_builder.CreateSRV(instance_buffer);
    parameters->PS.BarSizePixels = style.bar_size_pixels;
    parameters->PS.InsetPixels = style.inset_pixels;
    parameters->PS.BackgroundColor = FVector4f{style.background_color};
    parameters->PS.FillColor = FVector4f{style.fill_color};
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::EClear};

    auto const vertex_shader{
        TShaderMapRef<FEntityOverlayVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FEntityOverlayPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const instance_count{frame.instances.Num()};
    graph_builder.AddPass(
        RDG_EVENT_NAME("EntityOverlay.Draw Instances=%d", instance_count),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, instance_count, output_size = view.output_size](
            FRDGAsyncTask, FRHICommandList& command_list) {
            FGraphicsPipelineStateInitializer pipeline_state;
            command_list.ApplyCachedRenderTargets(pipeline_state);
            pipeline_state.BlendState = TStaticBlendState<>::GetRHI();
            pipeline_state.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
            pipeline_state.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
            pipeline_state.BoundShaderState.VertexDeclarationRHI =
                GEmptyVertexDeclaration.VertexDeclarationRHI;
            pipeline_state.BoundShaderState.VertexShaderRHI = vertex_shader.GetVertexShader();
            pipeline_state.BoundShaderState.PixelShaderRHI = pixel_shader.GetPixelShader();
            pipeline_state.PrimitiveType = PT_TriangleList;
            SetGraphicsPipelineState(command_list, pipeline_state, 0);
            command_list.SetViewport(0.0f, 0.0f, 0.0f, output_size.X, output_size.Y, 1.0f);
            command_list.SetStreamSource(0, nullptr, 0);
            SetShaderParameters(
                command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            SetShaderParameters(
                command_list, pixel_shader, pixel_shader.GetPixelShader(), parameters->PS);
            command_list.DrawPrimitive(0, 2, instance_count);
        });

    graph_builder.SetTextureAccessFinal(output_texture, ERHIAccess::SRVMask);
    graph_builder.Execute();
}
} // namespace ml::ui::entity_overlay

void FEntityOverlayRenderer::render(FEntityOverlayFramePtr frame,
                                    FEntityOverlayView const& view,
                                    FEntityOverlayStyle const& style,
                                    FTextureRenderTargetResource* const output_resource) const {
    check(IsInGameThread());
    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::PrepareUpload);
    if (!frame.IsValid() || !view.is_valid() || output_resource == nullptr) {
        UE_LOG(LogEntityOverlayRenderer, Error, TEXT("Cannot submit invalid entity overlay data."));
        return;
    }

    ENQUEUE_RENDER_COMMAND(RenderEntityOverlay)
    ([frame = MoveTemp(frame), view, style, output_resource](
         FRHICommandListImmediate& rhi_command_list) {
        auto const output_texture_rhi{output_resource->GetRenderTargetTexture()};
        if (!output_texture_rhi.IsValid()) {
            UE_LOG(LogEntityOverlayRenderer,
                   Error,
                   TEXT("The entity overlay output has no RHI texture."));
            return;
        }
        ml::ui::entity_overlay::execute_graph(
            rhi_command_list, *frame, view, style, output_texture_rhi);
    });
}

auto measure_entity_overlay_gpu(FRHICommandListImmediate& rhi_command_list,
                                FEntityOverlayFrame const& frame,
                                FEntityOverlayView const& view,
                                FEntityOverlayStyle const& style,
                                FTextureRenderTargetResource* const output_resource)
    -> TOptional<double> {
    check(IsInRenderingThread());
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
    ml::ui::entity_overlay::execute_graph(rhi_command_list, frame, view, style, output_texture_rhi);
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
