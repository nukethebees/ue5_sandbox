#include "Radar3DRenderer.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogRadar3DRenderer, Log, All);

namespace ml::ui::radar_3d {
constexpr uint32 static_line_count{18};

static_assert(sizeof(FRadar3DContact) == sizeof(float) * 8,
              "FRadar3DContact must match RadarContact in Radar3D.usf.");

class FRadarBackgroundVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarBackgroundVS);
    SHADER_USE_PARAMETER_STRUCT(FRadarBackgroundVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarBackgroundPS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarBackgroundPS);
    SHADER_USE_PARAMETER_STRUCT(FRadarBackgroundPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarPlaneVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarPlaneVS);
    SHADER_USE_PARAMETER_STRUCT(FRadarPlaneVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarPlanePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarPlanePS);
    SHADER_USE_PARAMETER_STRUCT(FRadarPlanePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarStaticLineVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarStaticLineVS);
    SHADER_USE_PARAMETER_STRUCT(FRadarStaticLineVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarContactLineVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarContactLineVS);
    SHADER_USE_PARAMETER_STRUCT(FRadarContactLineVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRadar3DContact>, Contacts)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarLinePS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarLinePS);
    SHADER_USE_PARAMETER_STRUCT(FRadarLinePS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarMarkerVS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarMarkerVS);
    SHADER_USE_PARAMETER_STRUCT(FRadarMarkerVS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRadar3DContact>, Contacts)
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FRadarMarkerPS : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FRadarMarkerPS);
    SHADER_USE_PARAMETER_STRUCT(FRadarMarkerPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()

    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FRadarBackgroundVS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_background_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRadarBackgroundPS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_background_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRadarPlaneVS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_plane_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRadarPlanePS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_plane_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRadarStaticLineVS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_static_line_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRadarContactLineVS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_contact_line_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRadarLinePS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_line_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRadarMarkerVS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_marker_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRadarMarkerPS,
                        "/Plugin/SandboxUI/Private/Radar3D/Radar3D.usf",
                        "radar_marker_ps",
                        SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FRadarBackgroundPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarBackgroundVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarBackgroundPS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRadarPlanePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarPlaneVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRadarStaticLinePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarStaticLineVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRadarContactLinePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarContactLineVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRadarMarkerPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FRadarMarkerVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void set_radar_pipeline(FRHICommandList& rhi_command_list,
                        FRHIVertexShader* const vertex_shader,
                        FRHIPixelShader* const pixel_shader,
                        bool const alpha_blend) {
    FGraphicsPipelineStateInitializer pipeline_state;
    rhi_command_list.ApplyCachedRenderTargets(pipeline_state);
    pipeline_state.BlendState = alpha_blend ? TStaticBlendState<CW_RGBA,
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

void set_radar_viewport(FRHICommandList& rhi_command_list, FIntPoint const output_size) {
    rhi_command_list.SetViewport(0.0f, 0.0f, 0.0f, output_size.X, output_size.Y, 1.0f);
    rhi_command_list.SetStreamSource(0, nullptr, 0);
}

void add_background_pass(FRDGBuilder& graph_builder,
                         FRDGTextureRef const output_texture,
                         FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FRadarBackgroundPassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->PS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ENoAction};

    auto const vertex_shader{
        TShaderMapRef<FRadarBackgroundVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{
        TShaderMapRef<FRadarBackgroundPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("Radar3D.Background"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_radar_pipeline(rhi_command_list,
                               vertex_shader.GetVertexShader(),
                               pixel_shader.GetPixelShader(),
                               false);
            set_radar_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            SetShaderParameters(
                rhi_command_list, pixel_shader, pixel_shader.GetPixelShader(), parameters->PS);
            rhi_command_list.DrawPrimitive(0, 2, 1);
        });
}

void add_plane_pass(FRDGBuilder& graph_builder,
                    FRDGTextureRef const output_texture,
                    FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FRadarPlanePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};

    auto const vertex_shader{TShaderMapRef<FRadarPlaneVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FRadarPlanePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("Radar3D.Plane"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_radar_pipeline(rhi_command_list,
                               vertex_shader.GetVertexShader(),
                               pixel_shader.GetPixelShader(),
                               true);
            set_radar_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            rhi_command_list.DrawPrimitive(0, 2, 1);
        });
}

void add_static_line_pass(FRDGBuilder& graph_builder,
                          FRDGTextureRef const output_texture,
                          FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FRadarStaticLinePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};

    auto const vertex_shader{
        TShaderMapRef<FRadarStaticLineVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FRadarLinePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(
        RDG_EVENT_NAME("Radar3D.StaticLines"),
        parameters,
        ERDGPassFlags::Raster,
        [parameters, vertex_shader, pixel_shader, output_size](FRDGAsyncTask,
                                                               FRHICommandList& rhi_command_list) {
            set_radar_pipeline(rhi_command_list,
                               vertex_shader.GetVertexShader(),
                               pixel_shader.GetPixelShader(),
                               true);
            set_radar_viewport(rhi_command_list, output_size);
            SetShaderParameters(
                rhi_command_list, vertex_shader, vertex_shader.GetVertexShader(), parameters->VS);
            rhi_command_list.DrawPrimitive(0, 2, static_line_count);
        });
}

void add_contact_line_pass(FRDGBuilder& graph_builder,
                           FRDGTextureRef const output_texture,
                           FRDGBufferSRVRef const contacts,
                           int32 const contact_count,
                           FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FRadarContactLinePassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->VS.Contacts = contacts;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};

    auto const vertex_shader{
        TShaderMapRef<FRadarContactLineVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FRadarLinePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(RDG_EVENT_NAME("Radar3D.ContactLines Contacts=%d", contact_count),
                          parameters,
                          ERDGPassFlags::Raster,
                          [parameters, vertex_shader, pixel_shader, output_size, contact_count](
                              FRDGAsyncTask, FRHICommandList& rhi_command_list) {
                              set_radar_pipeline(rhi_command_list,
                                                 vertex_shader.GetVertexShader(),
                                                 pixel_shader.GetPixelShader(),
                                                 true);
                              set_radar_viewport(rhi_command_list, output_size);
                              SetShaderParameters(rhi_command_list,
                                                  vertex_shader,
                                                  vertex_shader.GetVertexShader(),
                                                  parameters->VS);
                              rhi_command_list.DrawPrimitive(0, 2, contact_count);
                          });
}

void add_marker_pass(FRDGBuilder& graph_builder,
                     FRDGTextureRef const output_texture,
                     FRDGBufferSRVRef const contacts,
                     int32 const contact_count,
                     FIntPoint const output_size) {
    auto* const parameters{graph_builder.AllocParameters<FRadarMarkerPassParameters>()};
    parameters->VS.OutputSize = output_size;
    parameters->VS.Contacts = contacts;
    parameters->RenderTargets[0] =
        FRenderTargetBinding{output_texture, ERenderTargetLoadAction::ELoad};

    auto const vertex_shader{
        TShaderMapRef<FRadarMarkerVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const pixel_shader{TShaderMapRef<FRadarMarkerPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph_builder.AddPass(RDG_EVENT_NAME("Radar3D.Markers Contacts=%d", contact_count),
                          parameters,
                          ERDGPassFlags::Raster,
                          [parameters, vertex_shader, pixel_shader, output_size, contact_count](
                              FRDGAsyncTask, FRHICommandList& rhi_command_list) {
                              set_radar_pipeline(rhi_command_list,
                                                 vertex_shader.GetVertexShader(),
                                                 pixel_shader.GetPixelShader(),
                                                 true);
                              set_radar_viewport(rhi_command_list, output_size);
                              SetShaderParameters(rhi_command_list,
                                                  vertex_shader,
                                                  vertex_shader.GetVertexShader(),
                                                  parameters->VS);
                              rhi_command_list.DrawPrimitive(0, 2, contact_count);
                          });
}

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
    auto const contact_srv{graph_builder.CreateSRV(contact_buffer)};
    auto const output_texture{graph_builder.RegisterExternalTexture(
        CreateRenderTarget(output_texture_rhi, TEXT("Radar3D.Output")))};

    add_background_pass(graph_builder, output_texture, output_size);
    add_plane_pass(graph_builder, output_texture, output_size);
    add_static_line_pass(graph_builder, output_texture, output_size);
    add_contact_line_pass(graph_builder, output_texture, contact_srv, contacts.Num(), output_size);
    add_marker_pass(graph_builder, output_texture, contact_srv, contacts.Num(), output_size);

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
        ml::ui::radar_3d::render_radar_on_render_thread(
            rhi_command_list, contacts, output_resource);
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
    ml::ui::radar_3d::execute_radar_graph(rhi_command_list, contacts, output_texture_rhi);
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
