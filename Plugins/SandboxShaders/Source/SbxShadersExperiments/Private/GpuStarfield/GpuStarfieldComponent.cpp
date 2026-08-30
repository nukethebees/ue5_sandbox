#include "SbxShadersExperiments/GpuStarfield/GpuStarfieldComponent.h"

#include "Containers/ResourceArray.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "RHIResourceUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "VertexFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogGpuStarfieldExperiment, Log, All);

namespace {
constexpr int32 maximum_star_count{1000000};

auto get_profile_star_count_override() -> int32 {
    int32 star_count{0};
    FParse::Value(FCommandLine::Get(), TEXT("GpuStarfieldCount="), star_count);
    return star_count;
}

struct FGpuStarfieldRenderParameters {
    float global_star_size{0.0f};
    float global_brightness{0.0f};
    float parallax_scale{0.0f};
};

auto make_render_parameters(FGpuStarfieldSettings const& settings)
    -> FGpuStarfieldRenderParameters {
    return {
        .global_star_size = settings.global_star_size,
        .global_brightness = settings.global_brightness,
        .parallax_scale = settings.parallax_scale,
    };
}

class FGpuStarfieldQuadVertexBuffer : public FVertexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVector2f const vertices[]{
            {-1.0f, -1.0f},
            {1.0f, -1.0f},
            {-1.0f, 1.0f},
            {1.0f, 1.0f},
        };
        VertexBufferRHI =
            UE::RHIResourceUtils::CreateVertexBufferFromArray(rhi_command_list,
                                                              TEXT("GpuStarfield.QuadVertices"),
                                                              BUF_Static,
                                                              MakeConstArrayView(vertices));
    }
};

class FGpuStarfieldQuadIndexBuffer : public FIndexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        uint16 const indices[]{0, 2, 1, 1, 2, 3};
        IndexBufferRHI =
            UE::RHIResourceUtils::CreateIndexBufferFromArray(rhi_command_list,
                                                             TEXT("GpuStarfield.QuadIndices"),
                                                             BUF_Static,
                                                             MakeConstArrayView(indices));
    }
};

TGlobalResource<FGpuStarfieldQuadVertexBuffer> gpu_starfield_quad_vertex_buffer;
TGlobalResource<FGpuStarfieldQuadIndexBuffer> gpu_starfield_quad_index_buffer;

struct FGpuStarfieldBatchElementUserData final : public FOneFrameResource {
    FShaderResourceViewRHIRef star_data_srv;
    FGpuStarfieldRenderParameters parameters;
};

class FGpuStarfieldVertexFactoryShaderParameters final : public FVertexFactoryShaderParameters {
    DECLARE_TYPE_LAYOUT(FGpuStarfieldVertexFactoryShaderParameters, NonVirtual);
  public:
    void Bind(FShaderParameterMap const& parameter_map) {
        star_data_.Bind(parameter_map, TEXT("GpuStarfieldStarData"));
        global_star_size_.Bind(parameter_map, TEXT("GpuStarfieldGlobalStarSize"));
        global_brightness_.Bind(parameter_map, TEXT("GpuStarfieldGlobalBrightness"));
        parallax_scale_.Bind(parameter_map, TEXT("GpuStarfieldParallaxScale"));
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
    LAYOUT_FIELD(FShaderResourceParameter, star_data_);
    LAYOUT_FIELD(FShaderParameter, global_star_size_);
    LAYOUT_FIELD(FShaderParameter, global_brightness_);
    LAYOUT_FIELD(FShaderParameter, parallax_scale_);
};

IMPLEMENT_TYPE_LAYOUT(FGpuStarfieldVertexFactoryShaderParameters);

class FGpuStarfieldVertexFactory final : public FVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FGpuStarfieldVertexFactory);
  public:
    explicit FGpuStarfieldVertexFactory(ERHIFeatureLevel::Type const feature_level)
        : FVertexFactory{feature_level} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVertexDeclarationElementList elements;
        elements.Add(AccessStreamComponent(
            FVertexStreamComponent{
                &gpu_starfield_quad_vertex_buffer, 0, sizeof(FVector2f), VET_Float2},
            0));
        InitDeclaration(elements);
    }

    static auto
        ShouldCompilePermutation(FVertexFactoryShaderPermutationParameters const& parameters)
            -> bool {
        auto const& material{parameters.MaterialParameters};
        auto const is_starfield_material{material.MaterialDomain == MD_Surface &&
                                         material.BlendMode == BLEND_Additive &&
                                         material.ShadingModels.HasShadingModel(MSM_Unlit) &&
                                         material.bIsUsedWithParticleSprites};
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5) &&
               (is_starfield_material || material.bIsSpecialEngineMaterial);
    }

    static void
        ModifyCompilationEnvironment(FVertexFactoryShaderPermutationParameters const& parameters,
                                     FShaderCompilerEnvironment& environment) {
        FVertexFactory::ModifyCompilationEnvironment(parameters, environment);
    }
};

void FGpuStarfieldVertexFactoryShaderParameters::GetElementShaderBindings(
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
        static_cast<FGpuStarfieldBatchElementUserData const*>(batch_element.UserData)};
    check(user_data != nullptr);

    shader_bindings.Add(star_data_, user_data->star_data_srv);
    shader_bindings.Add(global_star_size_, user_data->parameters.global_star_size);
    shader_bindings.Add(global_brightness_, user_data->parameters.global_brightness);
    shader_bindings.Add(parallax_scale_, user_data->parameters.parallax_scale);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGpuStarfieldVertexFactory,
                                        SF_Vertex,
                                        FGpuStarfieldVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(
    FGpuStarfieldVertexFactory,
    "/Plugin/SandboxShaders/Private/GpuStarfield/GpuStarfieldVertexFactory.ush",
    EVertexFactoryFlags::UsedWithMaterials);

class FGpuStarfieldBuffer final : public FRenderResource {
  public:
    explicit FGpuStarfieldBuffer(TConstArrayView<FGpuStarfieldGpuData> const star_data)
        : initial_data_{star_data} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        if (initial_data_.IsEmpty()) {
            return;
        }

        FResourceArrayUploadArrayView upload_view{initial_data_};
        auto const buffer_size{initial_data_.Num() * sizeof(FGpuStarfieldGpuData)};
        auto const description{
            FRHIBufferCreateDesc::CreateStructured(
                TEXT("GpuStarfield.StarData"), buffer_size, sizeof(FGpuStarfieldGpuData))
                .AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static)
                .SetInitialState(ERHIAccess::SRVMask)
                .SetInitActionResourceArray(&upload_view)};
        buffer_ = rhi_command_list.CreateBuffer(description);
        srv_ = rhi_command_list.CreateShaderResourceView(
            buffer_, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(buffer_));
        initial_data_.Reset();
    }

    void ReleaseRHI() override {
        srv_.SafeRelease();
        buffer_.SafeRelease();
    }

    auto srv() const -> FShaderResourceViewRHIRef { return srv_; }
  private:
    TArray<FGpuStarfieldGpuData> initial_data_;
    FBufferRHIRef buffer_;
    FShaderResourceViewRHIRef srv_;
};

class FGpuStarfieldSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FGpuStarfieldSceneProxy(UGpuStarfieldComponent const* const component,
                            TConstArrayView<FGpuStarfieldGpuData> const star_data,
                            UMaterialInterface const* const material,
                            FGpuStarfieldRenderParameters const parameters)
        : FPrimitiveSceneProxy{component}
        , star_buffer_{star_data}
        , vertex_factory_{GetScene().GetFeatureLevel()}
        , material_render_proxy_{material->GetRenderProxy()}
        , material_relevance_{material->GetRelevance_Concurrent(GetScene().GetShaderPlatform())}
        , parameters_{parameters}
        , star_count_{star_data.Num()} {
        bWillEverBeLit = false;
        BeginInitResource(&star_buffer_);
        BeginInitResource(&vertex_factory_);
    }

    ~FGpuStarfieldSceneProxy() override {
        vertex_factory_.ReleaseResource();
        star_buffer_.ReleaseResource();
    }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const& view_family,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_GpuStarfieldSceneProxy_GetDynamicMeshElements);
        CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GpuStarfieldSubmit);

        auto const star_srv{star_buffer_.srv()};
        if (!star_srv.IsValid() || star_count_ <= 0) {
            return;
        }

        auto const view_count{views.Num()};
        for (int32 view_index{0}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto& user_data{
                collector.AllocateOneFrameResource<FGpuStarfieldBatchElementUserData>()};
            user_data.star_data_srv = star_srv;
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
            element.IndexBuffer = &gpu_starfield_quad_index_buffer;
            element.FirstIndex = 0;
            element.NumPrimitives = 2;
            element.NumInstances = star_count_;
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

    auto CanBeOccluded() const -> bool override { return !material_relevance_.bDisableDepthTest; }

    auto GetMemoryFootprint() const -> uint32 override {
        return sizeof(*this) + GetAllocatedSize();
    }

    auto GetAllocatedSize() const -> uint32 { return FPrimitiveSceneProxy::GetAllocatedSize(); }

    auto GetTypeHash() const -> SIZE_T override {
        static size_t unique_pointer;
        return reinterpret_cast<size_t>(&unique_pointer);
    }

    void set_parameters_render_thread(FGpuStarfieldRenderParameters const parameters) {
        check(IsInRenderingThread());
        parameters_ = parameters;
    }
  private:
    FGpuStarfieldBuffer star_buffer_;
    FGpuStarfieldVertexFactory vertex_factory_;
    FMaterialRenderProxy const* material_render_proxy_;
    FMaterialRelevance material_relevance_;
    FGpuStarfieldRenderParameters parameters_;
    int32 star_count_{0};
};
}

UGpuStarfieldComponent::UGpuStarfieldComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    bCastDynamicShadow = false;
    bCastStaticShadow = false;
    CanCharacterStepUpOn = ECB_No;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const material{
        TEXT("/SandboxShaders/Experiments/GpuStarfield/M_GpuStarfield.M_GpuStarfield")};
    if (material.Succeeded()) {
        material_ = material.Object;
    }
}

void UGpuStarfieldComponent::apply_settings(FGpuStarfieldSettings const& settings) {
    auto normalised{settings};
    auto const profile_star_count{get_profile_star_count_override()};
    if (profile_star_count > 0) {
        normalised.star_count = profile_star_count;
    }
    normalised.star_count = FMath::Clamp(normalised.star_count, 1, maximum_star_count);
    normalised.distribution_radius = FMath::Max(normalised.distribution_radius, 1.0f);
    normalised.global_star_size = FMath::Max(normalised.global_star_size, 0.0f);
    normalised.global_brightness = FMath::Max(normalised.global_brightness, 0.0f);
    normalised.parallax_scale = FMath::Clamp(normalised.parallax_scale, 0.0f, 1.0f);

    auto const structural_change{!has_generated_stars_ ||
                                 settings_.star_count != normalised.star_count ||
                                 settings_.random_seed != normalised.random_seed ||
                                 settings_.distribution_radius != normalised.distribution_radius};
    auto const bounds_change{settings_.global_star_size != normalised.global_star_size};
    auto const shader_parameter_change{settings_.global_star_size != normalised.global_star_size ||
                                       settings_.global_brightness !=
                                           normalised.global_brightness ||
                                       settings_.parallax_scale != normalised.parallax_scale};

    settings_ = normalised;
    if (structural_change) {
        generate_stars();
        UpdateBounds();
        MarkRenderStateDirty();
    } else if (shader_parameter_change) {
        if (bounds_change) {
            UpdateBounds();
            MarkRenderTransformDirty();
        }
        MarkRenderDynamicDataDirty();
    }
}

FPrimitiveSceneProxy* UGpuStarfieldComponent::CreateSceneProxy() {
    if (!IsValid(material_) || star_data_.IsEmpty()) {
        if (!IsValid(material_)) {
            UE_LOG(LogGpuStarfieldExperiment,
                   Warning,
                   TEXT("GPU starfield material is unavailable; regenerate the showcase assets."));
        }
        return nullptr;
    }

    return new FGpuStarfieldSceneProxy{
        this, star_data_, material_, make_render_parameters(settings_)};
}

FBoxSphereBounds UGpuStarfieldComponent::CalcBounds(FTransform const& local_to_world) const {
    auto const radius{settings_.distribution_radius + settings_.global_star_size};
    return FBoxSphereBounds{FVector::ZeroVector, FVector{radius}, radius}.TransformBy(
        local_to_world);
}

void UGpuStarfieldComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();

    if (SceneProxy == nullptr) {
        return;
    }

    auto* const scene_proxy{static_cast<FGpuStarfieldSceneProxy*>(SceneProxy)};
    auto const parameters{make_render_parameters(settings_)};
    ENQUEUE_RENDER_COMMAND(UpdateGpuStarfieldParameters)
    ([scene_proxy, parameters](FRHICommandListImmediate& rhi_command_list) {
        scene_proxy->set_parameters_render_thread(parameters);
    });
}

void UGpuStarfieldComponent::GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                                              bool const get_debug_materials) const {
    if (IsValid(material_)) {
        out_materials.AddUnique(material_);
    }
}

void UGpuStarfieldComponent::generate_stars() {
    FRandomStream random_stream{settings_.random_seed};
    star_data_.SetNumUninitialized(settings_.star_count);

    auto const star_count{star_data_.Num()};
    for (int32 star_index{0}; star_index < star_count; ++star_index) {
        auto const direction{random_stream.VRand()};
        auto& star{star_data_[star_index]};
        star.position = FVector3f{direction * settings_.distribution_radius};
        star.size = random_stream.FRandRange(0.75f, 1.25f);
        star.brightness = random_stream.FRandRange(0.5f, 1.0f);
        star.padding[0] = 0.0f;
        star.padding[1] = 0.0f;
        star.padding[2] = 0.0f;
    }

    has_generated_stars_ = true;
}
