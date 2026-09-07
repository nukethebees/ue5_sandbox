#include "RadarRenderer.h"

#include "SandboxUI/Radar/RadarBenchmarkSupport.h"

#include "CommonRenderResources.h"
#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadarRenderer, Log, All);

namespace ml::ui::radar {
inline constexpr uint32 structure_line_count{26};

class FPlaneVS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FPlaneVS);
    SHADER_USE_PARAMETER_STRUCT(FPlaneVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FPlanePS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FPlanePS);
    SHADER_USE_PARAMETER_STRUCT(FPlanePS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FVector4f, PlaneColor)
    SHADER_PARAMETER(FVector4f, StructureColor)
    SHADER_PARAMETER(float, GridOpacity)
    SHADER_PARAMETER(float, CoreCellRadius)
    SHADER_PARAMETER(float, CombatCellRadius)
    SHADER_PARAMETER(float, TacticalCellRadius)
    SHADER_PARAMETER(float, StrategicCellRadius)
    SHADER_PARAMETER(float, CombatDisplayRadius)
    SHADER_PARAMETER(float, TacticalDisplayRadius)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FStructureVS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FStructureVS);
    SHADER_USE_PARAMETER_STRUCT(FStructureVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(FVector4f, StructureColor)
    SHADER_PARAMETER(float, StructureOpacity)
    SHADER_PARAMETER(float, CombatDisplayRadius)
    SHADER_PARAMETER(float, TacticalDisplayRadius)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FStemVS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FStemVS);
    SHADER_USE_PARAMETER_STRUCT(FStemVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER(float, StemOpacity)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRadarInstance>, Instances)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FLinePS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FLinePS);
    SHADER_USE_PARAMETER_STRUCT(FLinePS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FGlyphVS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FGlyphVS);
    SHADER_USE_PARAMETER_STRUCT(FGlyphVS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FIntPoint, OutputSize)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FRadarInstance>, Instances)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

class FGlyphPS final : public FGlobalShader {
  public:
    DECLARE_GLOBAL_SHADER(FGlyphPS);
    SHADER_USE_PARAMETER_STRUCT(FGlyphPS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(FVector4f, ObjectiveColor)
    SHADER_PARAMETER(FVector4f, SelectionColor)
    SHADER_PARAMETER(FVector4f, PlayerColor)
    SHADER_PARAMETER(float, GlyphIntensity)
    SHADER_PARAMETER(float, ContactGlowOpacity)
    SHADER_PARAMETER(float, EmphasizedGlowOpacity)
    END_SHADER_PARAMETER_STRUCT()
    static auto ShouldCompilePermutation(FGlobalShaderPermutationParameters const& parameters)
        -> bool {
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FPlaneVS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_plane_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FPlanePS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_plane_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FStructureVS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_structure_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FStemVS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_stem_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FLinePS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_line_ps",
                        SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGlyphVS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_glyph_vs",
                        SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FGlyphPS,
                        "/Plugin/SandboxUI/Private/Radar/Radar.usf",
                        "radar_glyph_ps",
                        SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FPlanePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FPlaneVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FPlanePS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FStructurePassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FStructureVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FStemPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FStemVS::FParameters, VS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FGlyphPassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FGlyphVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FGlyphPS::FParameters, PS)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void set_pipeline(FRHICommandList& command_list,
                  FRHIVertexShader* const vertex_shader,
                  FRHIPixelShader* const pixel_shader) {
    FGraphicsPipelineStateInitializer state;
    command_list.ApplyCachedRenderTargets(state);
    state.BlendState = TStaticBlendState<CW_RGBA,
                                         BO_Add,
                                         BF_SourceAlpha,
                                         BF_InverseSourceAlpha,
                                         BO_Add,
                                         BF_One,
                                         BF_InverseSourceAlpha>::GetRHI();
    state.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
    state.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    state.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
    state.BoundShaderState.VertexShaderRHI = vertex_shader;
    state.BoundShaderState.PixelShaderRHI = pixel_shader;
    state.PrimitiveType = PT_TriangleList;
    SetGraphicsPipelineState(command_list, state, 0);
}

void set_viewport(FRHICommandList& command_list, FIntPoint const size) {
    command_list.SetViewport(0.0f, 0.0f, 0.0f, size.X, size.Y, 1.0f);
    command_list.SetStreamSource(0, nullptr, 0);
}

void execute_graph(FRHICommandListImmediate& command_list,
                   FRadarFrame const& frame,
                   FRadarStyle const& style,
                   FTextureRHIRef const& output_rhi) {
    auto const size{FIntPoint{static_cast<int32>(output_rhi->GetSizeX()),
                              static_cast<int32>(output_rhi->GetSizeY())}};
    FRDGBuilder graph{command_list};
    auto const output{
        graph.RegisterExternalTexture(CreateRenderTarget(output_rhi, TEXT("Radar.Output")))};

    auto* const plane{graph.AllocParameters<FPlanePassParameters>()};
    plane->VS.OutputSize = size;
    plane->PS.PlaneColor = FVector4f{style.plane_color};
    plane->PS.StructureColor = FVector4f{style.structure_color};
    plane->PS.GridOpacity = style.grid_opacity;
    plane->PS.CoreCellRadius = style.core_cell_radius;
    plane->PS.CombatCellRadius = style.combat_cell_radius;
    plane->PS.TacticalCellRadius = style.tactical_cell_radius;
    plane->PS.StrategicCellRadius = style.strategic_cell_radius;
    plane->PS.CombatDisplayRadius = frame.combat_display_radius;
    plane->PS.TacticalDisplayRadius = frame.tactical_display_radius;
    plane->RenderTargets[0] = FRenderTargetBinding{output, ERenderTargetLoadAction::EClear};
    auto const plane_vs{TShaderMapRef<FPlaneVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const plane_ps{TShaderMapRef<FPlanePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph.AddPass(RDG_EVENT_NAME("Radar.HexPlane"),
                  plane,
                  ERDGPassFlags::Raster,
                  [plane, plane_vs, plane_ps, size](FRDGAsyncTask, FRHICommandList& list) {
                      set_pipeline(list, plane_vs.GetVertexShader(), plane_ps.GetPixelShader());
                      set_viewport(list, size);
                      SetShaderParameters(list, plane_vs, plane_vs.GetVertexShader(), plane->VS);
                      SetShaderParameters(list, plane_ps, plane_ps.GetPixelShader(), plane->PS);
                      list.DrawPrimitive(0, 6, 1);
                  });

    auto* const structure{graph.AllocParameters<FStructurePassParameters>()};
    structure->VS.OutputSize = size;
    structure->VS.StructureColor = FVector4f{style.structure_color};
    structure->VS.StructureOpacity = style.structure_opacity;
    structure->VS.CombatDisplayRadius = frame.combat_display_radius;
    structure->VS.TacticalDisplayRadius = frame.tactical_display_radius;
    structure->RenderTargets[0] = FRenderTargetBinding{output, ERenderTargetLoadAction::ELoad};
    auto const structure_vs{TShaderMapRef<FStructureVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    auto const line_ps{TShaderMapRef<FLinePS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
    graph.AddPass(RDG_EVENT_NAME("Radar.Structure"),
                  structure,
                  ERDGPassFlags::Raster,
                  [structure, structure_vs, line_ps, size](FRDGAsyncTask, FRHICommandList& list) {
                      set_pipeline(list, structure_vs.GetVertexShader(), line_ps.GetPixelShader());
                      set_viewport(list, size);
                      SetShaderParameters(
                          list, structure_vs, structure_vs.GetVertexShader(), structure->VS);
                      list.DrawPrimitive(0, 2, structure_line_count);
                  });

    FRDGBufferSRVRef instances{nullptr};
    if (!frame.instances.IsEmpty()) {
        auto const buffer{CreateStructuredBuffer(graph,
                                                 TEXT("Radar.Instances"),
                                                 sizeof(FRadarInstance),
                                                 frame.instances.Num(),
                                                 frame.instances.GetData(),
                                                 frame.instances.Num() * sizeof(FRadarInstance),
                                                 ERDGInitialDataFlags::NoCopy)};
        instances = graph.CreateSRV(buffer);
    }

    auto const instance_count{frame.instances.Num()};
    if (instance_count > 0) {
        auto* const stems{graph.AllocParameters<FStemPassParameters>()};
        stems->VS.OutputSize = size;
        stems->VS.StemOpacity = style.stem_opacity;
        stems->VS.Instances = instances;
        stems->RenderTargets[0] = FRenderTargetBinding{output, ERenderTargetLoadAction::ELoad};
        auto const stem_vs{TShaderMapRef<FStemVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
        graph.AddPass(
            RDG_EVENT_NAME("Radar.Stems Instances=%d", instance_count),
            stems,
            ERDGPassFlags::Raster,
            [stems, stem_vs, line_ps, size, instance_count](FRDGAsyncTask, FRHICommandList& list) {
                set_pipeline(list, stem_vs.GetVertexShader(), line_ps.GetPixelShader());
                set_viewport(list, size);
                SetShaderParameters(list, stem_vs, stem_vs.GetVertexShader(), stems->VS);
                list.DrawPrimitive(0, 2, instance_count);
            });

        auto* const glyphs{graph.AllocParameters<FGlyphPassParameters>()};
        glyphs->VS.OutputSize = size;
        glyphs->VS.Instances = instances;
        glyphs->PS.ObjectiveColor = FVector4f{style.objective_color};
        glyphs->PS.SelectionColor = FVector4f{style.selection_color};
        glyphs->PS.PlayerColor = FVector4f{style.player_color};
        glyphs->PS.GlyphIntensity = style.glyph_intensity;
        glyphs->PS.ContactGlowOpacity = style.contact_glow_opacity;
        glyphs->PS.EmphasizedGlowOpacity = style.emphasized_glow_opacity;
        glyphs->RenderTargets[0] = FRenderTargetBinding{output, ERenderTargetLoadAction::ELoad};
        auto const glyph_vs{TShaderMapRef<FGlyphVS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
        auto const glyph_ps{TShaderMapRef<FGlyphPS>{GetGlobalShaderMap(GMaxRHIFeatureLevel)}};
        graph.AddPass(
            RDG_EVENT_NAME("Radar.Glyphs Instances=%d", instance_count),
            glyphs,
            ERDGPassFlags::Raster,
            [glyphs, glyph_vs, glyph_ps, size, instance_count](FRDGAsyncTask,
                                                               FRHICommandList& list) {
                set_pipeline(list, glyph_vs.GetVertexShader(), glyph_ps.GetPixelShader());
                set_viewport(list, size);
                SetShaderParameters(list, glyph_vs, glyph_vs.GetVertexShader(), glyphs->VS);
                SetShaderParameters(list, glyph_ps, glyph_ps.GetPixelShader(), glyphs->PS);
                list.DrawPrimitive(0, 2, instance_count);
            });
    }

    graph.SetTextureAccessFinal(output, ERHIAccess::SRVMask);
    graph.Execute();
}
} // namespace ml::ui::radar

void FRadarRenderer::render(FRadarFrameStoreConstPtr frame_store,
                            FRadarStyle const& style,
                            FTextureRenderTargetResource* const output_resource) const {
    check(IsInGameThread());
    if (!frame_store.IsValid() || output_resource == nullptr) {
        UE_LOG(LogRadarRenderer, Error, TEXT("Cannot submit invalid radar data."));
        return;
    }

    auto const* const frame{&frame_store->current()};
    ENQUEUE_RENDER_COMMAND(RenderRadar)
    ([frame_store = MoveTemp(frame_store), frame, style, output_resource](
         FRHICommandListImmediate& list) {
        static_cast<void>(frame_store);
        auto const output_rhi{output_resource->GetRenderTargetTexture()};
        if (!output_rhi.IsValid()) {
            UE_LOG(LogRadarRenderer, Error, TEXT("The radar output has no RHI texture."));
            return;
        }
        ml::ui::radar::execute_graph(list, *frame, style, output_rhi);
    });
}

void submit_radar_render(FRadarFrameStoreConstPtr frame_store,
                         FRadarStyle const& style,
                         FTextureRenderTargetResource* const output_resource) {
    FRadarRenderer{}.render(MoveTemp(frame_store), style, output_resource);
}

auto measure_radar_gpu(FRHICommandListImmediate& command_list,
                       FRadarFrame const& frame,
                       FRadarStyle const& style,
                       FTextureRenderTargetResource* const output_resource) -> TOptional<double> {
    check(IsInRenderingThread());
    if (!GSupportsTimestampRenderQueries || output_resource == nullptr) {
        return {};
    }
    auto const output_rhi{output_resource->GetRenderTargetTexture()};
    if (!output_rhi.IsValid()) {
        return {};
    }

    auto const query_pool{RHICreateRenderQueryPool(RQT_AbsoluteTime, 2)};
    auto start_query{query_pool->AllocateQuery()};
    auto end_query{query_pool->AllocateQuery()};
    command_list.EndRenderQuery(start_query.GetQuery());
    ml::ui::radar::execute_graph(command_list, frame, style, output_rhi);
    command_list.EndRenderQuery(end_query.GetQuery());
    command_list.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

    uint64 start_microseconds{0};
    uint64 end_microseconds{0};
    if (!RHIGetRenderQueryResult(start_query.GetQuery(), start_microseconds, true) ||
        !RHIGetRenderQueryResult(end_query.GetQuery(), end_microseconds, true) ||
        end_microseconds < start_microseconds) {
        return {};
    }
    return static_cast<double>(end_microseconds - start_microseconds);
}
