#include "SpaceGameRendering/SparkRendererComponent.h"

#include "Containers/ResourceArray.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "RHIResourceUtils.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "UObject/ConstructorHelpers.h"
#include "VertexFactory.h"

TRACE_DECLARE_INT_COUNTER(SandboxSparkUploadBytes, TEXT("Sandbox/Sparks/UploadBytes"));
TRACE_DECLARE_FLOAT_COUNTER(SandboxSparkRenderThreadUploadMs,
                            TEXT("Sandbox/Sparks/RenderThreadUploadMs"));

namespace SpaceGame::Sparks::Private {
inline constexpr int32 maximum_capacity{1000000};

struct FRenderParameters {
    float effect_time{0.0f};
    FVector3f acceleration{FVector3f::ZeroVector};
    float maximum_draw_distance{0.0f};
    float minimum_thickness_pixels{0.0f};
    float maximum_thickness_pixels{0.0f};
    float maximum_length_pixels{0.0f};
    float latest_expiry_time{0.0f};
};

struct FRenderUploadRange {
    int32 first_index{0};
    TArray<FSparkParticleRecord> particles;
};

class FQuadVertexBuffer : public FVertexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVector2f const vertices[]{{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f}};
        VertexBufferRHI =
            UE::RHIResourceUtils::CreateVertexBufferFromArray(rhi_command_list,
                                                              TEXT("Sparks.QuadVertices"),
                                                              BUF_Static,
                                                              MakeConstArrayView(vertices));
    }
};

class FQuadIndexBuffer : public FIndexBuffer {
  public:
    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        uint16 const indices[]{0, 2, 1, 1, 2, 3};
        IndexBufferRHI = UE::RHIResourceUtils::CreateIndexBufferFromArray(
            rhi_command_list, TEXT("Sparks.QuadIndices"), BUF_Static, MakeConstArrayView(indices));
    }
};

TGlobalResource<FQuadVertexBuffer> quad_vertex_buffer;
TGlobalResource<FQuadIndexBuffer> quad_index_buffer;

struct FBatchElementUserData final : public FOneFrameResource {
    FShaderResourceViewRHIRef particle_data_srv;
    FRenderParameters parameters;
};

class FSparkVertexFactoryShaderParameters final : public FVertexFactoryShaderParameters {
    DECLARE_TYPE_LAYOUT(FSparkVertexFactoryShaderParameters, NonVirtual);
  public:
    void Bind(FShaderParameterMap const& parameter_map) {
        particle_data_.Bind(parameter_map, TEXT("SparkParticleData"));
        effect_time_.Bind(parameter_map, TEXT("SparkEffectTime"));
        acceleration_.Bind(parameter_map, TEXT("SparkAcceleration"));
        maximum_draw_distance_.Bind(parameter_map, TEXT("SparkMaximumDrawDistance"));
        minimum_thickness_pixels_.Bind(parameter_map, TEXT("SparkMinimumThicknessPixels"));
        maximum_thickness_pixels_.Bind(parameter_map, TEXT("SparkMaximumThicknessPixels"));
        maximum_length_pixels_.Bind(parameter_map, TEXT("SparkMaximumLengthPixels"));
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
    LAYOUT_FIELD(FShaderResourceParameter, particle_data_);
    LAYOUT_FIELD(FShaderParameter, effect_time_);
    LAYOUT_FIELD(FShaderParameter, acceleration_);
    LAYOUT_FIELD(FShaderParameter, maximum_draw_distance_);
    LAYOUT_FIELD(FShaderParameter, minimum_thickness_pixels_);
    LAYOUT_FIELD(FShaderParameter, maximum_thickness_pixels_);
    LAYOUT_FIELD(FShaderParameter, maximum_length_pixels_);
};

IMPLEMENT_TYPE_LAYOUT(FSparkVertexFactoryShaderParameters);

class FSparkVertexFactory final : public FVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FSparkVertexFactory);
  public:
    explicit FSparkVertexFactory(ERHIFeatureLevel::Type const feature_level)
        : FVertexFactory{feature_level} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        FVertexDeclarationElementList elements;
        elements.Add(AccessStreamComponent(
            FVertexStreamComponent{&quad_vertex_buffer, 0, sizeof(FVector2f), VET_Float2}, 0));
        InitDeclaration(elements);
    }

    static auto
        ShouldCompilePermutation(FVertexFactoryShaderPermutationParameters const& parameters)
            -> bool {
        auto const& material{parameters.MaterialParameters};
        auto const is_spark_material{material.MaterialDomain == MD_Surface &&
                                     material.BlendMode == BLEND_Additive &&
                                     material.ShadingModels.HasShadingModel(MSM_Unlit) &&
                                     material.bIsUsedWithParticleSprites};
        return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5) &&
               (is_spark_material || material.bIsSpecialEngineMaterial);
    }
};

void FSparkVertexFactoryShaderParameters::GetElementShaderBindings(
    FSceneInterface const* const scene,
    FSceneView const* const view,
    FMeshMaterialShader const* const shader,
    EVertexInputStreamType const input_stream_type,
    ERHIFeatureLevel::Type const feature_level,
    ::FVertexFactory const* const vertex_factory,
    FMeshBatchElement const& batch_element,
    FMeshDrawSingleShaderBindings& shader_bindings,
    FVertexInputStreamArray& vertex_streams) const {
    auto const* const user_data{static_cast<FBatchElementUserData const*>(batch_element.UserData)};
    check(user_data != nullptr);
    shader_bindings.Add(particle_data_, user_data->particle_data_srv);
    shader_bindings.Add(effect_time_, user_data->parameters.effect_time);
    shader_bindings.Add(acceleration_, user_data->parameters.acceleration);
    shader_bindings.Add(maximum_draw_distance_, user_data->parameters.maximum_draw_distance);
    shader_bindings.Add(minimum_thickness_pixels_, user_data->parameters.minimum_thickness_pixels);
    shader_bindings.Add(maximum_thickness_pixels_, user_data->parameters.maximum_thickness_pixels);
    shader_bindings.Add(maximum_length_pixels_, user_data->parameters.maximum_length_pixels);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSparkVertexFactory,
                                        SF_Vertex,
                                        FSparkVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSparkVertexFactory,
                              "/Plugin/SpaceGame/Private/Sparks/SparkVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials);

class FParticleBuffer final : public FRenderResource {
  public:
    explicit FParticleBuffer(TConstArrayView<FSparkParticleRecord> const initial_data)
        : initial_data_{initial_data} {}

    void InitRHI(FRHICommandListBase& rhi_command_list) override {
        if (initial_data_.IsEmpty()) {
            return;
        }
        FResourceArrayUploadArrayView upload_view{initial_data_};
        auto const buffer_size{initial_data_.Num() * sizeof(FSparkParticleRecord)};
        auto const description{
            FRHIBufferCreateDesc::CreateStructured(
                TEXT("Sparks.ParticleData"), buffer_size, sizeof(FSparkParticleRecord))
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

    void upload(FRHICommandListBase& rhi_command_list,
                TConstArrayView<FRenderUploadRange> const ranges) {
        auto const start_cycles{FPlatformTime::Cycles64()};
        int64 upload_bytes{0};
        for (auto const& range : ranges) {
            auto const byte_count{range.particles.Num() * sizeof(FSparkParticleRecord)};
            if (byte_count <= 0) {
                continue;
            }
            auto const byte_offset{range.first_index * sizeof(FSparkParticleRecord)};
            auto* const destination{
                rhi_command_list.LockBuffer(buffer_, byte_offset, byte_count, RLM_WriteOnly)};
            FMemory::Memcpy(destination, range.particles.GetData(), byte_count);
            rhi_command_list.UnlockBuffer(buffer_);
            upload_bytes += byte_count;
        }
        TRACE_COUNTER_SET_ALWAYS(SandboxSparkUploadBytes, upload_bytes);
        TRACE_COUNTER_SET_ALWAYS(
            SandboxSparkRenderThreadUploadMs,
            FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - start_cycles));
    }

    auto srv() const -> FShaderResourceViewRHIRef { return srv_; }
  private:
    TArray<FSparkParticleRecord> initial_data_;
    FBufferRHIRef buffer_;
    FShaderResourceViewRHIRef srv_;
};

class FSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FSceneProxy(USparkRendererComponent const* const component,
                TConstArrayView<FSparkParticleRecord> const particles,
                UMaterialInterface const* const material,
                FRenderParameters const parameters)
        : FPrimitiveSceneProxy{component}
        , particle_buffer_{particles}
        , vertex_factory_{GetScene().GetFeatureLevel()}
        , material_render_proxy_{material->GetRenderProxy()}
        , material_relevance_{material->GetRelevance_Concurrent(GetScene().GetShaderPlatform())}
        , parameters_{parameters}
        , particle_count_{particles.Num()} {
        bWillEverBeLit = false;
        bIsAlwaysVisible = true;
        BeginInitResource(&particle_buffer_);
        BeginInitResource(&vertex_factory_);
    }

    ~FSceneProxy() override {
        vertex_factory_.ReleaseResource();
        particle_buffer_.ReleaseResource();
    }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const& view_family,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_SparkSceneProxy_GetDynamicMeshElements);
        CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SparkSubmit);
        auto const particle_srv{particle_buffer_.srv()};
        if (!particle_srv.IsValid() || particle_count_ <= 0 ||
            parameters_.effect_time >= parameters_.latest_expiry_time) {
            return;
        }

        auto const view_count{views.Num()};
        for (int32 view_index{0}; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }
            auto& user_data{collector.AllocateOneFrameResource<FBatchElementUserData>()};
            user_data.particle_data_srv = particle_srv;
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
            element.IndexBuffer = &quad_index_buffer;
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

    auto CanBeOccluded() const -> bool override { return !material_relevance_.bDisableDepthTest; }
    auto GetMemoryFootprint() const -> uint32 override {
        return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
    }
    auto GetTypeHash() const -> SIZE_T override {
        static size_t unique_pointer;
        return reinterpret_cast<size_t>(&unique_pointer);
    }

    void update(FRHICommandListBase& rhi_command_list,
                TConstArrayView<FRenderUploadRange> const uploads,
                FRenderParameters const parameters) {
        check(IsInRenderingThread());
        particle_buffer_.upload(rhi_command_list, uploads);
        parameters_ = parameters;
    }
  private:
    FParticleBuffer particle_buffer_;
    FSparkVertexFactory vertex_factory_;
    FMaterialRenderProxy const* material_render_proxy_;
    FMaterialRelevance material_relevance_;
    FRenderParameters parameters_;
    int32 particle_count_{0};
};

auto make_parameters(USparkRendererComponent const& component,
                     FSparkRendererSettings const& settings,
                     float const effect_time,
                     float const latest_expiry_time) -> FRenderParameters {
    auto const local_acceleration{FVector3f{
        component.GetComponentTransform().InverseTransformVector(FVector{settings.acceleration})}};
    return {
        .effect_time = effect_time,
        .acceleration = local_acceleration,
        .maximum_draw_distance = settings.maximum_draw_distance,
        .minimum_thickness_pixels = settings.minimum_thickness_pixels,
        .maximum_thickness_pixels = settings.maximum_thickness_pixels,
        .maximum_length_pixels = settings.maximum_length_pixels,
        .latest_expiry_time = latest_expiry_time,
    };
}
} // namespace SpaceGame::Sparks::Private

USparkRendererComponent::USparkRendererComponent() {
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

void USparkRendererComponent::initialise(FSparkRendererSettings const& settings) {
    settings_ = settings;
    auto const defaults{FSparkRendererSettings{}};
    settings_.capacity =
        FMath::Clamp(settings_.capacity, 1, SpaceGame::Sparks::Private::maximum_capacity);
    settings_.maximum_draw_distance = FMath::IsFinite(settings_.maximum_draw_distance)
                                        ? FMath::Max(0.0f, settings_.maximum_draw_distance)
                                        : defaults.maximum_draw_distance;
    settings_.acceleration =
        settings_.acceleration.ContainsNaN() ? defaults.acceleration : settings_.acceleration;
    settings_.minimum_thickness_pixels = FMath::IsFinite(settings_.minimum_thickness_pixels)
                                           ? FMath::Max(0.0f, settings_.minimum_thickness_pixels)
                                           : defaults.minimum_thickness_pixels;
    settings_.maximum_thickness_pixels = FMath::IsFinite(settings_.maximum_thickness_pixels)
                                           ? settings_.maximum_thickness_pixels
                                           : defaults.maximum_thickness_pixels;
    settings_.maximum_thickness_pixels =
        FMath::Max(settings_.minimum_thickness_pixels, settings_.maximum_thickness_pixels);
    settings_.maximum_length_pixels = FMath::IsFinite(settings_.maximum_length_pixels)
                                        ? FMath::Max(0.0f, settings_.maximum_length_pixels)
                                        : defaults.maximum_length_pixels;
    particle_data_.Reset();
    particle_data_.SetNumZeroed(settings_.capacity);
    pending_upload_first_indices_.Reset();
    pending_upload_particles_.Reset();
    allocation_cursor_ = 0;
    effect_time_ = 0.0f;
    latest_expiry_time_ = 0.0f;
    UpdateBounds();
    MarkRenderStateDirty();
}

void USparkRendererComponent::clear_sparks() {
    FMemory::Memzero(particle_data_.GetData(), particle_data_.Num() * sizeof(FSparkParticleRecord));
    pending_upload_first_indices_.Reset();
    pending_upload_particles_.Reset();
    allocation_cursor_ = 0;
    effect_time_ = 0.0f;
    latest_expiry_time_ = 0.0f;
    MarkRenderStateDirty();
}

auto
    USparkRendererComponent::submit_particles(TConstArrayView<FSparkParticleRecord> const particles,
                                              float const effect_time) -> int32 {
    check(IsInGameThread());
    effect_time_ = effect_time;
    if (particles.IsEmpty() || particle_data_.IsEmpty()) {
        MarkRenderDynamicDataDirty();
        return 0;
    }

    auto const count{FMath::Min(particles.Num(), particle_data_.Num())};
    auto overwritten{0};
    auto const first_count{FMath::Min(count, particle_data_.Num() - allocation_cursor_)};
    auto const append_range =
        [this, &particles, &overwritten, effect_time](
            int32 const destination, int32 const source, int32 const range_count) {
            if (range_count <= 0) {
                return;
            }
            for (int32 index{0}; index < range_count; ++index) {
                auto const& old_particle{particle_data_[destination + index]};
                auto const old_expiry{old_particle.initial_position_spawn_time.W +
                                      old_particle.initial_velocity_lifetime.W};
                if (old_particle.initial_velocity_lifetime.W > 0.0f && old_expiry > effect_time) {
                    ++overwritten;
                }
            }
            FMemory::Memcpy(particle_data_.GetData() + destination,
                            particles.GetData() + source,
                            range_count * sizeof(FSparkParticleRecord));
            pending_upload_first_indices_.Add(destination);
            auto& upload_particles{pending_upload_particles_.AddDefaulted_GetRef()};
            upload_particles.Append(particles.GetData() + source, range_count);
        };

    append_range(allocation_cursor_, 0, first_count);
    append_range(0, first_count, count - first_count);
    allocation_cursor_ = (allocation_cursor_ + count) % particle_data_.Num();
    for (int32 index{0}; index < count; ++index) {
        auto const& particle{particles[index]};
        latest_expiry_time_ = FMath::Max(latest_expiry_time_,
                                         particle.initial_position_spawn_time.W +
                                             particle.initial_velocity_lifetime.W);
    }
    MarkRenderDynamicDataDirty();
    return overwritten;
}

FPrimitiveSceneProxy* USparkRendererComponent::CreateSceneProxy() {
    if (!IsValid(material_) || particle_data_.IsEmpty()) {
        return nullptr;
    }
    return new SpaceGame::Sparks::Private::FSceneProxy{
        this,
        particle_data_,
        material_,
        SpaceGame::Sparks::Private::make_parameters(
            *this, settings_, effect_time_, latest_expiry_time_)};
}

FBoxSphereBounds USparkRendererComponent::CalcBounds(FTransform const& local_to_world) const {
    auto const radius{static_cast<double>(FMath::Max(settings_.maximum_draw_distance, 1.0f))};
    return FBoxSphereBounds{FVector::ZeroVector, FVector{radius}, radius}.TransformBy(
        local_to_world);
}

void USparkRendererComponent::SendRenderDynamicData_Concurrent() {
    Super::SendRenderDynamicData_Concurrent();
    if (SceneProxy == nullptr) {
        pending_upload_first_indices_.Reset();
        pending_upload_particles_.Reset();
        return;
    }

    check(pending_upload_first_indices_.Num() == pending_upload_particles_.Num());
    TArray<SpaceGame::Sparks::Private::FRenderUploadRange> uploads;
    auto const upload_count{pending_upload_particles_.Num()};
    uploads.Reserve(upload_count);
    for (int32 index{0}; index < upload_count; ++index) {
        uploads.Add({.first_index = pending_upload_first_indices_[index],
                     .particles = MoveTemp(pending_upload_particles_[index])});
    }
    pending_upload_first_indices_.Reset();
    pending_upload_particles_.Reset();
    auto const parameters{SpaceGame::Sparks::Private::make_parameters(
        *this, settings_, effect_time_, latest_expiry_time_)};
    auto* const scene_proxy{static_cast<SpaceGame::Sparks::Private::FSceneProxy*>(SceneProxy)};
    ENQUEUE_RENDER_COMMAND(UpdateSparkRenderer)
    ([scene_proxy, uploads = MoveTemp(uploads), parameters](
         FRHICommandListImmediate& command_list) {
        scene_proxy->update(command_list, uploads, parameters);
    });
}

void USparkRendererComponent::GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                                               bool const get_debug_materials) const {
    if (IsValid(material_)) {
        out_materials.AddUnique(material_);
    }
}
