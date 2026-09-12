#include "SpaceGameRendering/SparkRendererComponent.h"

#include "GpuSparkBurst.h"
#include "SparkParticleBuffer.h"
#include "SparkPendingGpuBatches.h"
#include "SparkStagingState.h"
#include "SparkUploadBuffer.h"

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

namespace SpaceGame::Sparks::Private {
inline constexpr int32 maximum_capacity{1000000};
inline constexpr uint32 direct_particle_indices{MAX_uint32};

struct FRenderParameters {
    float effect_time{0.0f};
    FVector3f acceleration{FVector3f::ZeroVector};
    float maximum_draw_distance{0.0f};
    float minimum_thickness_pixels{0.0f};
    float maximum_thickness_pixels{0.0f};
    float maximum_length_pixels{0.0f};
    float latest_expiry_time{0.0f};
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

class FSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FSceneProxy(USparkRendererComponent const* const component,
                FSparkParticleBuffer& particle_buffer,
                int32 const particle_count,
                UMaterialInterface const* const material,
                FRenderParameters const parameters)
        : FPrimitiveSceneProxy{component}
        , particle_buffer_{&particle_buffer}
        , vertex_factory_{GetScene().GetFeatureLevel()}
        , material_render_proxy_{material->GetRenderProxy()}
        , material_relevance_{material->GetRelevance_Concurrent(GetScene().GetShaderPlatform())}
        , parameters_{parameters}
        , particle_count_{particle_count} {
        bWillEverBeLit = false;
        bIsAlwaysVisible = true;
        BeginInitResource(&vertex_factory_);
    }

    ~FSceneProxy() override { vertex_factory_.ReleaseResource(); }

    void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                FSceneViewFamily const& view_family,
                                uint32 const visibility_map,
                                FMeshElementCollector& collector) const override {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_SparkSceneProxy_GetDynamicMeshElements);
        CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SparkSubmit);
        auto const particle_srv{particle_buffer_->srv()};
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

    void update(FRHICommandListImmediate& rhi_command_list,
                FSparkUploadBuffer* const upload_buffer,
                TArray<FGpuSparkBurst>&& gpu_bursts,
                TArray<uint32>&& selection_indices,
                bool const clear,
                FRenderParameters const parameters) {
        check(IsInRenderingThread());
        if (clear) {
            particle_buffer_->clear(rhi_command_list);
        }
        if (upload_buffer != nullptr) {
            particle_buffer_->upload(rhi_command_list, *upload_buffer);
            upload_buffer->in_flight.Store(false);
        }
        particle_buffer_->expand(
            rhi_command_list, MoveTemp(gpu_bursts), MoveTemp(selection_indices));
        parameters_ = parameters;
    }
  private:
    FSparkParticleBuffer* particle_buffer_{nullptr};
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

auto valid_particle_count(FSparkBurst const& burst) -> int32 {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto const ranges_are_finite{
        FMath::IsFinite(style.speed.min) && FMath::IsFinite(style.speed.max) &&
        FMath::IsFinite(style.lifetime.min) && FMath::IsFinite(style.lifetime.max) &&
        FMath::IsFinite(style.size.min) && FMath::IsFinite(style.size.max)};
    auto const valid{style.count > 0 && !emission.location.ContainsNaN() &&
                     !emission.direction.ContainsNaN() && !emission.colour.ContainsNaN() &&
                     ranges_are_finite && FMath::IsFinite(style.intensity) &&
                     style.intensity > 0.0f && FMath::IsFinite(style.spread_angle_degrees) &&
                     FMath::IsFinite(style.streak_time) &&
                     FMath::Max(style.lifetime.min, style.lifetime.max) > 0.0f};
    return valid ? style.count : 0;
}

auto make_gpu_burst(FSparkBurst const& burst,
                    float const effect_time,
                    int32 const first_particle,
                    int32 const particle_count,
                    uint32 const selection_first) -> FGpuSparkBurst {
    auto const& emission{burst.emission};
    auto const& style{burst.style};
    auto const direction{emission.direction.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector)};
    auto const spread_radians{
        FMath::DegreesToRadians(FMath::Clamp(style.spread_angle_degrees, 0.0f, 180.0f))};
    return {
        .location_spawn_time =
            FVector4f{emission.location.X, emission.location.Y, emission.location.Z, effect_time},
        .direction_cone_cosine =
            FVector4f{direction.X, direction.Y, direction.Z, FMath::Cos(spread_radians)},
        .colour_intensity =
            FVector4f{emission.colour.X, emission.colour.Y, emission.colour.Z, style.intensity},
        .speed_lifetime_ranges =
            FVector4f{style.speed.min, style.speed.max, style.lifetime.min, style.lifetime.max},
        .size_streak_reserved = FVector4f{style.size.min, style.size.max, style.streak_time, 0.0f},
        .allocation = FUintVector4{emission.seed,
                                   static_cast<uint32>(first_particle),
                                   static_cast<uint32>(particle_count),
                                   selection_first},
    };
}
} // namespace SpaceGame::Sparks::Private

USparkRendererComponent::USparkRendererComponent()
    : staging_state_{MakeShared<FSparkStagingState, ESPMode::ThreadSafe>()}
    , pending_gpu_batches_{new FSparkPendingGpuBatches{}}
    , particle_buffer_{new FSparkParticleBuffer{}} {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    bCastDynamicShadow = false;
    bCastStaticShadow = false;
    CanCharacterStepUpOn = ECB_No;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const material{
        TEXT("/SandboxShaders/GpuStarfield/M_GpuStarfield.M_GpuStarfield")};
    if (material.Succeeded()) {
        material_ = material.Object;
    }
}

USparkRendererComponent::~USparkRendererComponent() {
    if (particle_buffer_ && particle_buffer_->IsInitialized()) {
        ReleaseResourceAndFlush(particle_buffer_);
    }
    delete particle_buffer_;
    delete pending_gpu_batches_;
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
    if (particle_buffer_->IsInitialized()) {
        FlushRenderingCommands();
    }
    particle_buffer_->set_initial_data(particle_data_);
    if (particle_buffer_->IsInitialized()) {
        BeginUpdateResourceRHI(particle_buffer_);
    } else {
        BeginInitResource(particle_buffer_);
    }
    staging_state_ = MakeShared<FSparkStagingState, ESPMode::ThreadSafe>();
    pending_upload_buffer_ = nullptr;
    pending_gpu_batches_->bursts.Reset();
    pending_gpu_batches_->selection_indices.Reset();
    allocation_cursor_ = 0;
    allocated_slot_count_ = 0;
    last_submission_upload_bytes_ = 0;
    effect_time_ = 0.0f;
    latest_expiry_time_ = 0.0f;
    clear_pending_ = false;
    UpdateBounds();
    MarkRenderStateDirty();
}

void USparkRendererComponent::clear_sparks() {
    FMemory::Memzero(particle_data_.GetData(), particle_data_.Num() * sizeof(FSparkParticleRecord));
    staging_state_ = MakeShared<FSparkStagingState, ESPMode::ThreadSafe>();
    pending_upload_buffer_ = nullptr;
    pending_gpu_batches_->bursts.Reset();
    pending_gpu_batches_->selection_indices.Reset();
    allocation_cursor_ = 0;
    allocated_slot_count_ = 0;
    last_submission_upload_bytes_ = 0;
    effect_time_ = 0.0f;
    latest_expiry_time_ = 0.0f;
    clear_pending_ = true;
    MarkRenderDynamicDataDirty();
}

auto USparkRendererComponent::submit_bursts(TConstArrayView<FSparkBurst> const bursts,
                                            float const effect_time) -> FSparkSubmissionResult {
    check(IsInGameThread());
    effect_time_ = effect_time;
    last_submission_upload_bytes_ = 0;
    auto const capacity{particle_data_.Num()};
    if (bursts.IsEmpty() || capacity <= 0) {
        MarkRenderDynamicDataDirty();
        return {};
    }

    TArray<int32, TInlineAllocator<64>> burst_counts;
    burst_counts.SetNumUninitialized(bursts.Num());
    int64 requested_count{0};
    auto const burst_count{bursts.Num()};
    for (int32 index{0}; index < burst_count; ++index) {
        burst_counts[index] = SpaceGame::Sparks::Private::valid_particle_count(bursts[index]);
        requested_count += burst_counts[index];
    }
    auto const admitted_count{static_cast<int32>(FMath::Min<int64>(requested_count, capacity))};
    if (admitted_count <= 0) {
        MarkRenderDynamicDataDirty();
        return {.requested = requested_count};
    }

    auto const unallocated_slots{capacity - allocated_slot_count_};
    auto const replaced_slots{FMath::Max(admitted_count - unallocated_slots, 0)};
    allocated_slot_count_ = FMath::Min(capacity, allocated_slot_count_ + admitted_count);
    auto& pending{*pending_gpu_batches_};
    auto const initial_burst_count{pending.bursts.Num()};
    auto const initial_selection_count{pending.selection_indices.Num()};
    auto cursor{allocation_cursor_};
    if (requested_count <= capacity) {
        for (int32 index{0}; index < burst_count; ++index) {
            auto const count{burst_counts[index]};
            if (count <= 0) {
                continue;
            }
            pending.bursts.Add(SpaceGame::Sparks::Private::make_gpu_burst(
                bursts[index],
                effect_time,
                cursor,
                count,
                SpaceGame::Sparks::Private::direct_particle_indices));
            cursor = (cursor + count) % capacity;
            latest_expiry_time_ =
                FMath::Max(latest_expiry_time_,
                           effect_time + FMath::Max(bursts[index].style.lifetime.min,
                                                    bursts[index].style.lifetime.max));
        }
    } else {
        int32 burst_index{0};
        int64 burst_start{0};
        FGpuSparkBurst* gpu_burst{nullptr};
        auto const quotient{requested_count / admitted_count};
        auto const remainder{requested_count % admitted_count};
        for (int32 admitted_index{0}; admitted_index < admitted_count; ++admitted_index) {
            auto const half_remainder_numerator{(quotient % 2) * admitted_count +
                                                (static_cast<int64>(admitted_index) * 2 + 1) *
                                                    remainder};
            auto const flattened_index{static_cast<int64>(admitted_index) * quotient +
                                       quotient / 2 +
                                       half_remainder_numerator / (admitted_count * 2)};
            auto const previous_burst_index{burst_index};
            while (burst_index + 1 < burst_count &&
                   flattened_index >= burst_start + burst_counts[burst_index]) {
                burst_start += burst_counts[burst_index];
                ++burst_index;
            }
            if (gpu_burst == nullptr || burst_index != previous_burst_index) {
                gpu_burst = &pending.bursts.Add_GetRef(SpaceGame::Sparks::Private::make_gpu_burst(
                    bursts[burst_index],
                    effect_time,
                    cursor,
                    0,
                    static_cast<uint32>(pending.selection_indices.Num())));
                latest_expiry_time_ =
                    FMath::Max(latest_expiry_time_,
                               effect_time + FMath::Max(bursts[burst_index].style.lifetime.min,
                                                        bursts[burst_index].style.lifetime.max));
            }
            pending.selection_indices.Add(static_cast<uint32>(flattened_index - burst_start));
            ++gpu_burst->allocation.Z;
            cursor = (cursor + 1) % capacity;
        }
    }
    allocation_cursor_ = cursor;
    last_submission_upload_bytes_ =
        (pending.bursts.Num() - initial_burst_count) * sizeof(FGpuSparkBurst) +
        (pending.selection_indices.Num() - initial_selection_count) * sizeof(uint32);
    MarkRenderDynamicDataDirty();
    return {
        .requested = requested_count, .admitted = admitted_count, .replaced_slots = replaced_slots};
}

auto
    USparkRendererComponent::submit_particles(TConstArrayView<FSparkParticleRecord> const particles,
                                              float const effect_time) -> int32 {
    check(IsInGameThread());
    effect_time_ = effect_time;
    last_submission_upload_bytes_ = 0;
    if (particles.IsEmpty() || particle_data_.IsEmpty()) {
        MarkRenderDynamicDataDirty();
        return 0;
    }

    auto const count{FMath::Min(particles.Num(), particle_data_.Num())};
    last_submission_upload_bytes_ = count * sizeof(FSparkParticleRecord);
    auto overwritten{0};
    if (pending_upload_buffer_ == nullptr) {
        auto& upload_buffer{staging_state_->buffers.next()};
        if (upload_buffer.in_flight.Load()) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                USparkRendererComponent::submit_particles::WaitForUploadBuffer);
            FlushRenderingCommands();
            check(!upload_buffer.in_flight.Load());
        }
        upload_buffer.particles.Reset();
        upload_buffer.destinations.Reset();
        upload_buffer.counts.Reset();
        pending_upload_buffer_ = &upload_buffer;
    }
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
            pending_upload_buffer_->destinations.Add(destination);
            pending_upload_buffer_->counts.Add(range_count);
            pending_upload_buffer_->particles.Append(particles.GetData() + source, range_count);
        };

    append_range(allocation_cursor_, 0, first_count);
    append_range(0, first_count, count - first_count);
    allocation_cursor_ = (allocation_cursor_ + count) % particle_data_.Num();
    allocated_slot_count_ = FMath::Min(particle_data_.Num(), allocated_slot_count_ + count);
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
    if (!IsValid(material_) || particle_data_.IsEmpty() || !particle_buffer_) {
        return nullptr;
    }
    return new SpaceGame::Sparks::Private::FSceneProxy{
        this,
        *particle_buffer_,
        particle_data_.Num(),
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
        return;
    }

    auto* const upload_buffer{pending_upload_buffer_};
    pending_upload_buffer_ = nullptr;
    if (upload_buffer != nullptr) {
        staging_state_->buffers.cycle();
        check(&staging_state_->buffers.current() == upload_buffer);
        check(!upload_buffer->in_flight.Load());
        upload_buffer->in_flight.Store(true);
    }
    auto const parameters{SpaceGame::Sparks::Private::make_parameters(
        *this, settings_, effect_time_, latest_expiry_time_)};
    auto gpu_bursts{MoveTemp(pending_gpu_batches_->bursts)};
    auto selection_indices{MoveTemp(pending_gpu_batches_->selection_indices)};
    auto const clear{clear_pending_};
    clear_pending_ = false;
    auto* const scene_proxy{static_cast<SpaceGame::Sparks::Private::FSceneProxy*>(SceneProxy)};
    auto staging_state{staging_state_};
    ENQUEUE_RENDER_COMMAND(UpdateSparkRenderer)
    ([scene_proxy,
      upload_buffer,
      staging_state = MoveTemp(staging_state),
      gpu_bursts = MoveTemp(gpu_bursts),
      selection_indices = MoveTemp(selection_indices),
      clear,
      parameters](FRHICommandListImmediate& command_list) mutable {
        static_cast<void>(staging_state);
        scene_proxy->update(command_list,
                            upload_buffer,
                            MoveTemp(gpu_bursts),
                            MoveTemp(selection_indices),
                            clear,
                            parameters);
    });
}

void USparkRendererComponent::GetUsedMaterials(TArray<UMaterialInterface*>& out_materials,
                                               bool const get_debug_materials) const {
    if (IsValid(material_)) {
        out_materials.AddUnique(material_);
    }
}
