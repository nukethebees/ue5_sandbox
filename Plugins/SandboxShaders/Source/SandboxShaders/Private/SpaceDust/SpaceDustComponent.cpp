#include "SandboxShaders/SpaceDust/SpaceDustComponent.h"

#include <sandbox/core/space_dust_math.h>

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "RHIResourceUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "UObject/ConstructorHelpers.h"
#include "VertexFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpaceDust, Log, All);

namespace {
struct FSpaceDustRenderParameters {
    FVector3f volume_dimensions{FVector3f::OneVector};
    FVector3f translation_phase{FVector3f::ZeroVector};
    FVector3f world_velocity{FVector3f::ZeroVector};
    FVector3f colour{FVector3f::OneVector};
    uint32 random_seed{};
    float particle_size{};
    float brightness{};
    float minimum_visible_speed{};
    float full_visible_speed{};
    float streak_seconds{};
    float minimum_motion_pixels{};
    float full_motion_pixels{};
    float maximum_streak_pixels{};
    float volume_edge_fade_fraction{};
};

auto make_render_parameters(FSpaceDustSettings const& settings,
                            FVector3f const translation_phase,
                            FVector3f const world_velocity) -> FSpaceDustRenderParameters {
    return {
        .volume_dimensions = FVector3f{settings.volume_dimensions},
        .translation_phase = translation_phase,
        .world_velocity = world_velocity,
        .colour = FVector3f{settings.colour},
        .random_seed = static_cast<uint32>(settings.random_seed),
        .particle_size = settings.particle_size,
        .brightness = settings.brightness,
        .minimum_visible_speed = settings.minimum_visible_speed,
        .full_visible_speed = settings.full_visible_speed,
        .streak_seconds = settings.streak_seconds,
        .minimum_motion_pixels = settings.minimum_motion_pixels,
        .full_motion_pixels = settings.full_motion_pixels,
        .maximum_streak_pixels = settings.maximum_streak_pixels,
        .volume_edge_fade_fraction = settings.volume_edge_fade_fraction,
    };
}

class FSpaceDustQuadVertexBuffer : public FVertexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVector2f const vertices[]{{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
        VertexBufferRHI =
            UE::RHIResourceUtils::CreateVertexBufferFromArray(rhi_command_list,
                                                              TEXT("SpaceDust.QuadVertices"),
                                                              BUF_Static,
                                                              MakeConstArrayView(vertices));
    }
};

class FSpaceDustQuadIndexBuffer : public FIndexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        uint16 const indices[]{0, 2, 1, 1, 2, 3};
        IndexBufferRHI =
            UE::RHIResourceUtils::CreateIndexBufferFromArray(rhi_command_list,
                                                             TEXT("SpaceDust.QuadIndices"),
                                                             BUF_Static,
                                                             MakeConstArrayView(indices));
    }
};

TGlobalResource<FSpaceDustQuadVertexBuffer> space_dust_quad_vertex_buffer;
TGlobalResource<FSpaceDustQuadIndexBuffer> space_dust_quad_index_buffer;

struct FSpaceDustBatchElementUserData final : public FOneFrameResource {
    FSpaceDustRenderParameters parameters;
};

class FSpaceDustVertexFactoryShaderParameters final : public FVertexFactoryShaderParameters {
    DECLARE_TYPE_LAYOUT(FSpaceDustVertexFactoryShaderParameters, NonVirtual);
  public:
    void Bind(FShaderParameterMap const& parameter_map) {
        volume_dimensions_.Bind(parameter_map, TEXT("SpaceDustVolumeDimensions"));
        translation_phase_.Bind(parameter_map, TEXT("SpaceDustTranslationPhase"));
        world_velocity_.Bind(parameter_map, TEXT("SpaceDustWorldVelocity"));
        colour_.Bind(parameter_map, TEXT("SpaceDustColour"));
        random_seed_.Bind(parameter_map, TEXT("SpaceDustRandomSeed"));
        particle_size_.Bind(parameter_map, TEXT("SpaceDustParticleSize"));
        brightness_.Bind(parameter_map, TEXT("SpaceDustBrightness"));
        minimum_visible_speed_.Bind(parameter_map, TEXT("SpaceDustMinimumVisibleSpeed"));
        full_visible_speed_.Bind(parameter_map, TEXT("SpaceDustFullVisibleSpeed"));
        streak_seconds_.Bind(parameter_map, TEXT("SpaceDustStreakSeconds"));
        minimum_motion_pixels_.Bind(parameter_map, TEXT("SpaceDustMinimumMotionPixels"));
        full_motion_pixels_.Bind(parameter_map, TEXT("SpaceDustFullMotionPixels"));
        maximum_streak_pixels_.Bind(parameter_map, TEXT("SpaceDustMaximumStreakPixels"));
        volume_edge_fade_fraction_.Bind(parameter_map, TEXT("SpaceDustVolumeEdgeFadeFraction"));
    }

    void GetElementShaderBindings(FSceneInterface const* scene,
                                  FSceneView const* view,
                                  FMeshMaterialShader const* shader,
                                  EVertexInputStreamType input_stream_type,
                                  ERHIFeatureLevel::Type feature_level,
                                  FVertexFactory const* vertex_factory,
                                  FMeshBatchElement const& batch_element,
                                  FMeshDrawSingleShaderBindings& shader_bindings,
                                  FVertexInputStreamArray& vertex_streams) const;
  private:
    LAYOUT_FIELD(FShaderParameter, volume_dimensions_);
    LAYOUT_FIELD(FShaderParameter, translation_phase_);
    LAYOUT_FIELD(FShaderParameter, world_velocity_);
    LAYOUT_FIELD(FShaderParameter, colour_);
    LAYOUT_FIELD(FShaderParameter, random_seed_);
    LAYOUT_FIELD(FShaderParameter, particle_size_);
    LAYOUT_FIELD(FShaderParameter, brightness_);
    LAYOUT_FIELD(FShaderParameter, minimum_visible_speed_);
    LAYOUT_FIELD(FShaderParameter, full_visible_speed_);
    LAYOUT_FIELD(FShaderParameter, streak_seconds_);
    LAYOUT_FIELD(FShaderParameter, minimum_motion_pixels_);
    LAYOUT_FIELD(FShaderParameter, full_motion_pixels_);
    LAYOUT_FIELD(FShaderParameter, maximum_streak_pixels_);
    LAYOUT_FIELD(FShaderParameter, volume_edge_fade_fraction_);
};

IMPLEMENT_TYPE_LAYOUT(FSpaceDustVertexFactoryShaderParameters);

class FSpaceDustVertexFactory final : public FVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FSpaceDustVertexFactory);
  public:
    explicit FSpaceDustVertexFactory(ERHIFeatureLevel::Type const feature_level)
        : FVertexFactory{feature_level} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVertexDeclarationElementList elements;
        elements.Add(AccessStreamComponent(
            FVertexStreamComponent{
                &space_dust_quad_vertex_buffer, 0, sizeof(FVector2f), VET_Float2},
            0));
        InitDeclaration(elements);
    }

    static auto
        ShouldCompilePermutation(FVertexFactoryShaderPermutationParameters const& parameters)
            -> bool {
        auto const& material{parameters.MaterialParameters};
        auto const is_space_dust_material{material.MaterialDomain == MD_Surface &&
                                          material.BlendMode == BLEND_Additive &&
                                          material.ShadingModels.HasShadingModel(MSM_Unlit)};
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5) &&
               (is_space_dust_material || material.bIsSpecialEngineMaterial);
    }
};

void FSpaceDustVertexFactoryShaderParameters::GetElementShaderBindings(
    FSceneInterface const* const scene,
    FSceneView const* const view,
    FMeshMaterialShader const* const shader,
    EVertexInputStreamType const input_stream_type,
    ERHIFeatureLevel::Type const feature_level,
    FVertexFactory const* const vertex_factory,
    FMeshBatchElement const& batch_element,
    FMeshDrawSingleShaderBindings& shader_bindings,
    FVertexInputStreamArray& vertex_streams) const {
    auto const* const user_data{
        static_cast<FSpaceDustBatchElementUserData const*>(batch_element.UserData)};
    check(user_data != nullptr);

    auto const& parameters{user_data->parameters};
    shader_bindings.Add(volume_dimensions_, parameters.volume_dimensions);
    shader_bindings.Add(translation_phase_, parameters.translation_phase);
    shader_bindings.Add(world_velocity_, parameters.world_velocity);
    shader_bindings.Add(colour_, parameters.colour);
    shader_bindings.Add(random_seed_, parameters.random_seed);
    shader_bindings.Add(particle_size_, parameters.particle_size);
    shader_bindings.Add(brightness_, parameters.brightness);
    shader_bindings.Add(minimum_visible_speed_, parameters.minimum_visible_speed);
    shader_bindings.Add(full_visible_speed_, parameters.full_visible_speed);
    shader_bindings.Add(streak_seconds_, parameters.streak_seconds);
    shader_bindings.Add(minimum_motion_pixels_, parameters.minimum_motion_pixels);
    shader_bindings.Add(full_motion_pixels_, parameters.full_motion_pixels);
    shader_bindings.Add(maximum_streak_pixels_, parameters.maximum_streak_pixels);
    shader_bindings.Add(volume_edge_fade_fraction_, parameters.volume_edge_fade_fraction);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSpaceDustVertexFactory,
                                        SF_Vertex,
                                        FSpaceDustVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSpaceDustVertexFactory,
                              "/Plugin/SandboxShaders/Private/SpaceDust/SpaceDustVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials);

class FSpaceDustSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FSpaceDustSceneProxy(USpaceDustComponent const* const component,
                         UMaterialInterface const* const material,
                         FSpaceDustRenderParameters const parameters,
                         int32 const particle_count)
        : FPrimitiveSceneProxy{component}
        , vertex_factory_{GetScene().GetFeatureLevel()}
        , material_render_proxy_{material->GetRenderProxy()}
        , material_relevance_{material->GetRelevance_Concurrent(GetScene().GetShaderPlatform())}
        , parameters_{parameters}
        , particle_count_{particle_count} {
        bWillEverBeLit = false;
        BeginInitResource(&vertex_factory_);
    }

    ~FSpaceDustSceneProxy() override { vertex_factory_.ReleaseResource(); }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const& view_family,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_SpaceDustSceneProxy_GetDynamicMeshElements);
        CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SpaceDustSubmit);

        if (particle_count_ <= 0) {
            return;
        }

        auto const view_count{views.Num()};
        for (int32 view_index{}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto& user_data{collector.AllocateOneFrameResource<FSpaceDustBatchElementUserData>()};
            user_data.parameters = parameters_;

            auto& mesh{collector.AllocateMesh()};
            mesh.VertexFactory = &vertex_factory_;
            mesh.MaterialRenderProxy = material_render_proxy_;
            mesh.Type = PT_TriangleList;
            mesh.DepthPriorityGroup = SDPG_World;
            mesh.CastShadow = false;
            mesh.bUseAsOccluder = false;
            mesh.bCanApplyViewModeOverrides = false;
            mesh.bDisableBackfaceCulling = true;

            auto& element{mesh.Elements[0]};
            element.IndexBuffer = &space_dust_quad_index_buffer;
            element.FirstIndex = 0;
            element.NumPrimitives = 2;
            element.NumInstances = particle_count_;
            element.MinVertexIndex = 0;
            element.MaxVertexIndex = 3;
            element.PrimitiveUniformBuffer = GetUniformBuffer();
            element.UserData = &user_data;

            collector.AddMesh(view_index, mesh);
        }
    }

    auto GetViewRelevance(FSceneView const* const view) const -> FPrimitiveViewRelevance override {
        FPrimitiveViewRelevance relevance;
        relevance.bDrawRelevance = IsShown(view);
        relevance.bDynamicRelevance = true;
        relevance.bShadowRelevance = false;
        relevance.bRenderInMainPass = ShouldRenderInMainPass();
        material_relevance_.SetPrimitiveViewRelevance(relevance);
        relevance.bVelocityRelevance = false;
        return relevance;
    }

    auto CanBeOccluded() const -> bool override { return false; }
    auto GetMemoryFootprint() const -> uint32 override {
        return sizeof(*this) + GetAllocatedSize();
    }
    auto GetAllocatedSize() const -> uint32 { return FPrimitiveSceneProxy::GetAllocatedSize(); }
    auto GetTypeHash() const -> SIZE_T override {
        static size_t unique_pointer;
        return reinterpret_cast<size_t>(&unique_pointer);
    }

    void set_parameters_render_thread(FSpaceDustRenderParameters const parameters,
                                      int32 const particle_count) {
        check(IsInRenderingThread());
        parameters_ = parameters;
        particle_count_ = particle_count;
    }
  private:
    FSpaceDustVertexFactory vertex_factory_;
    FMaterialRenderProxy const* material_render_proxy_;
    FMaterialRelevance material_relevance_;
    FSpaceDustRenderParameters parameters_;
    int32 particle_count_{};
};

auto to_native_settings(FSpaceDustSettings const& settings) -> ml::space_dust::Tuning {
    return {
        .enabled = settings.enabled,
        .particle_count = settings.particle_count,
        .random_seed = static_cast<uint32>(FMath::Max(settings.random_seed, 0)),
        .volume_dimensions = ml::make_vector3f(static_cast<float>(settings.volume_dimensions.X),
                                               static_cast<float>(settings.volume_dimensions.Y),
                                               static_cast<float>(settings.volume_dimensions.Z)),
        .particle_size = settings.particle_size,
        .brightness = settings.brightness,
        .colour = ml::make_vector3f(settings.colour.R, settings.colour.G, settings.colour.B),
        .minimum_visible_speed = settings.minimum_visible_speed,
        .full_visible_speed = settings.full_visible_speed,
        .streak_seconds = settings.streak_seconds,
        .minimum_motion_pixels = settings.minimum_motion_pixels,
        .full_motion_pixels = settings.full_motion_pixels,
        .maximum_streak_pixels = settings.maximum_streak_pixels,
        .volume_edge_fade_fraction = settings.volume_edge_fade_fraction,
    };
}

auto apply_native_settings(FSpaceDustSettings settings, ml::space_dust::Tuning const& native)
    -> FSpaceDustSettings {
    settings.enabled = native.enabled;
    settings.particle_count = native.particle_count;
    settings.random_seed = static_cast<int32>(native.random_seed);
    settings.volume_dimensions =
        FVector{native.volume_dimensions.X, native.volume_dimensions.Y, native.volume_dimensions.Z};
    settings.particle_size = native.particle_size;
    settings.brightness = native.brightness;
    settings.colour.R = native.colour.X;
    settings.colour.G = native.colour.Y;
    settings.colour.B = native.colour.Z;
    settings.colour.A = 1.0f;
    settings.minimum_visible_speed = native.minimum_visible_speed;
    settings.full_visible_speed = native.full_visible_speed;
    settings.streak_seconds = native.streak_seconds;
    settings.minimum_motion_pixels = native.minimum_motion_pixels;
    settings.full_motion_pixels = native.full_motion_pixels;
    settings.maximum_streak_pixels = native.maximum_streak_pixels;
    settings.volume_edge_fade_fraction = native.volume_edge_fade_fraction;
    return settings;
}
}

auto normalise_space_dust_settings(FSpaceDustSettings settings) -> FSpaceDustSettings {
    return apply_native_settings(settings,
                                 ml::space_dust::normalise_tuning(to_native_settings(settings)));
}

auto make_space_dust_translation_phase(FVector const world_location,
                                       FVector const volume_dimensions) -> FVector3f {
    auto const phase{ml::space_dust::make_translation_phase(
        {.x = world_location.X, .y = world_location.Y, .z = world_location.Z},
        ml::make_vector3f(static_cast<float>(volume_dimensions.X),
                          static_cast<float>(volume_dimensions.Y),
                          static_cast<float>(volume_dimensions.Z)))};
    return FVector3f{phase.X, phase.Y, phase.Z};
}

USpaceDustComponent::USpaceDustComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    bCastDynamicShadow = false;
    bCastStaticShadow = false;
    bOnlyOwnerSee = true;
    bHiddenInSceneCapture = true;
    bVisibleInReflectionCaptures = false;
    bVisibleInRealTimeSkyCaptures = false;
    bVisibleInRayTracing = false;
    bVisibleInReflections = false;
    CanCharacterStepUpOn = ECB_No;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const material{
        TEXT("/SandboxShaders/Generated/Materials/M_SpaceDust.M_SpaceDust")};
    if (material.Succeeded()) {
        material_ = material.Object;
    }
}

void USpaceDustComponent::apply_settings(FSpaceDustSettings const& settings) {
    auto const normalised{normalise_space_dust_settings(settings)};
    auto const bounds_change{settings_.volume_dimensions != normalised.volume_dimensions};

    settings_ = normalised;
    update_translation_phase();
    if (bounds_change) {
        UpdateBounds();
        MarkRenderTransformDirty();
    }
    MarkRenderDynamicDataDirty();
}

void USpaceDustComponent::update_motion(FVector const world_velocity) {
    world_velocity_ = FVector3f{world_velocity};
    update_translation_phase();
    MarkRenderDynamicDataDirty();
}

FPrimitiveSceneProxy* USpaceDustComponent::CreateSceneProxy() {
    if (!IsValid(material_)) {
        UE_LOG(LogSpaceDust, Warning, TEXT("Space dust material is unavailable."));
        return nullptr;
    }

    return new FSpaceDustSceneProxy{
        this,
        material_,
        make_render_parameters(settings_, translation_phase_, world_velocity_),
        settings_.enabled ? settings_.particle_count : 0};
}

FBoxSphereBounds USpaceDustComponent::CalcBounds(FTransform const& local_to_world) const {
    auto const extent{settings_.volume_dimensions * 0.5};
    return FBoxSphereBounds{local_to_world.GetLocation(), extent, extent.Length()};
}

void USpaceDustComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();
    if (SceneProxy == nullptr) {
        return;
    }

    auto* const scene_proxy{static_cast<FSpaceDustSceneProxy*>(SceneProxy)};
    auto const parameters{make_render_parameters(settings_, translation_phase_, world_velocity_)};
    auto const particle_count{settings_.enabled ? settings_.particle_count : 0};
    ENQUEUE_RENDER_COMMAND(UpdateSpaceDustParameters)
    ([scene_proxy, parameters, particle_count](FRHICommandListImmediate& rhi_command_list) {
        scene_proxy->set_parameters_render_thread(parameters, particle_count);
    });
}

void USpaceDustComponent::GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                                           bool const get_debug_materials) const {
    if (IsValid(material_)) {
        out_materials.AddUnique(material_);
    }
}

void USpaceDustComponent::update_translation_phase() {
    translation_phase_ =
        make_space_dust_translation_phase(GetComponentLocation(), settings_.volume_dimensions);
}
