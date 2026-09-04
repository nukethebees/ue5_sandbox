#include "SandboxISMCComponent.h"

#include "Engine/InstancedStaticMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Rendering/ColorVertexBuffer.h"
#include "RenderResource.h"
#include "RHICommandList.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SandboxISMCComponent)

DEFINE_LOG_CATEGORY_STATIC(LogSandboxISMC, Log, All);

DECLARE_STATS_GROUP(TEXT("SandboxISMC"), STATGROUP_SandboxISMC, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Prepare instance update"), STAT_SandboxISMCPrepare, STATGROUP_SandboxISMC);
DECLARE_CYCLE_STAT(TEXT("Submit instance update"), STAT_SandboxISMCSubmit, STATGROUP_SandboxISMC);
DECLARE_CYCLE_STAT(TEXT("Upload instance buffer"), STAT_SandboxISMCUpload, STATGROUP_SandboxISMC);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active instances"),
                           STAT_SandboxISMCInstances,
                           STATGROUP_SandboxISMC);
DECLARE_DWORD_COUNTER_STAT(TEXT("Last upload bytes"),
                           STAT_SandboxISMCUploadBytes,
                           STATGROUP_SandboxISMC);

TRACE_DECLARE_FLOAT_COUNTER(SandboxISMCRenderThreadUploadMs,
                            TEXT("SandboxISMC/RenderThreadUploadMs"));
TRACE_DECLARE_MEMORY_COUNTER(SandboxISMCRenderThreadUploadBytes,
                             TEXT("SandboxISMC/RenderThreadUploadBytes"));
TRACE_DECLARE_INT_COUNTER(SandboxISMCRenderThreadUploadInstances,
                          TEXT("SandboxISMC/RenderThreadUploadInstances"));

struct FSandboxISMCMetricsState {
    TAtomic<int32> instance_count{0};
    TAtomic<uint64> prepare_cycles{0};
    TAtomic<uint64> pack_cycles{0};
    TAtomic<uint64> bounds_cycles{0};
    TAtomic<uint64> submit_cycles{0};
    TAtomic<uint64> upload_cycles{0};
    TAtomic<uint64> upload_bytes{0};
    TAtomic<int32> dirty_instance_count{0};
    TAtomic<int32> dirty_range_count{0};
};

struct FSandboxISMCRenderInstance {
    FVector4f origin;
    FVector4f transform_row_0;
    FVector4f transform_row_1;
    FVector4f transform_row_2;
};

static_assert(sizeof(FSandboxISMCRenderInstance) == 64);

struct FSandboxISMCRenderRange {
    int32 first_instance{0};
    int32 count{0};
};

struct FSandboxISMCRenderUpdate {
    int32 instance_count{0};
    bool full_upload{false};
    TArray<FSandboxISMCRenderRange> ranges;
    TArray<FSandboxISMCRenderInstance> instances;
};

namespace {
class FSandboxISMCInstanceBuffer final : public FVertexBuffer {
  public:
    FSandboxISMCInstanceBuffer(
        TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> initial_update,
        TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics)
        : initial_update_{MoveTemp(initial_update)}
        , metrics_{MoveTemp(metrics)} {}

    virtual FString GetFriendlyName() const override {
        return TEXT("Sandbox ISMC instance buffer");
    }

    virtual void InitRHI(FRHICommandListBase& rhi_command_list) override {
        auto const initial_count = initial_update_.IsValid() ? initial_update_->instance_count : 0;
        allocate(rhi_command_list, FMath::Max(initial_count, 1));

        if (initial_update_.IsValid()) {
            upload(rhi_command_list, *initial_update_);
            initial_update_.Reset();
        }
    }

    int32 get_initial_instance_count() const {
        return initial_update_.IsValid() ? initial_update_->instance_count : 0;
    }

    void upload(FRHICommandListBase& rhi_command_list, FSandboxISMCRenderUpdate const& update) {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_RenderThreadUpload);
        SCOPE_CYCLE_COUNTER(STAT_SandboxISMCUpload);
        auto const start_cycles = FPlatformTime::Cycles64();
        auto const instance_count = update.instance_count;

        if (instance_count > capacity_) {
            check(update.full_upload);
            allocate(rhi_command_list, instance_count);
        }

        uint64 byte_count{0};
        int32 uploaded_instance_count{0};
        int32 source_instance_index{0};
        for (auto const& range : update.ranges) {
            auto const range_instance_count{range.count};
            auto const range_byte_count{
                static_cast<uint32>(range_instance_count * sizeof(FSandboxISMCRenderInstance))};
            if (range_byte_count == 0) {
                continue;
            }

            auto const byte_offset{
                static_cast<uint32>(range.first_instance * sizeof(FSandboxISMCRenderInstance))};
            check(range.first_instance >= 0);
            check(range.first_instance + range_instance_count <= instance_count);
            check(source_instance_index + range_instance_count <= update.instances.Num());
            auto* destination = rhi_command_list.LockBuffer(
                VertexBufferRHI, byte_offset, range_byte_count, RLM_WriteOnly);
            FMemory::Memcpy(
                destination, update.instances.GetData() + source_instance_index, range_byte_count);
            rhi_command_list.UnlockBuffer(VertexBufferRHI);
            byte_count += range_byte_count;
            uploaded_instance_count += range_instance_count;
            source_instance_index += range_instance_count;
        }
        check(source_instance_index == update.instances.Num());

        auto const elapsed_cycles = FPlatformTime::Cycles64() - start_cycles;
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadMs,
                                 FPlatformTime::ToMilliseconds64(elapsed_cycles));
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadBytes,
                                 static_cast<int64>(byte_count));
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadInstances, uploaded_instance_count);
        metrics_->upload_cycles.Store(elapsed_cycles);
        SET_DWORD_STAT(STAT_SandboxISMCUploadBytes,
                       static_cast<uint32>(FMath::Min<uint64>(byte_count, MAX_uint32)));
    }
  private:
    void allocate(FRHICommandListBase& rhi_command_list, int32 required_capacity) {
        auto const requested_capacity = FMath::Max(required_capacity, 1);
        capacity_ = FMath::RoundUpToPowerOfTwo(requested_capacity);

        VertexBufferRHI.SafeRelease();
        auto const create_description =
            FRHIBufferCreateDesc::CreateVertex<FSandboxISMCRenderInstance>(
                TEXT("SandboxISMC.InstanceBuffer"), capacity_)
                .DetermineInitialState();
        VertexBufferRHI = rhi_command_list.CreateBuffer(create_description);
    }

    int32 capacity_{0};
    TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> initial_update_;
    TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics_;
};

class FSandboxISMCVertexFactory final : public FLocalVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FSandboxISMCVertexFactory);
  public:
    FSandboxISMCVertexFactory(ERHIFeatureLevel::Type feature_level,
                              FSandboxISMCInstanceBuffer const* instance_buffer)
        : FLocalVertexFactory{feature_level, "FSandboxISMCVertexFactory"}
        , instance_buffer_{instance_buffer} {}

    void set_static_mesh_data(FDataType const& data) { Data = data; }

    static bool
        ShouldCompilePermutation(FVertexFactoryShaderPermutationParameters const& parameters) {
        return parameters.MaterialParameters.MaterialDomain == MD_Surface &&
               (parameters.MaterialParameters.bIsUsedWithInstancedStaticMeshes ||
                parameters.MaterialParameters.bIsSpecialEngineMaterial);
    }

    static void
        ModifyCompilationEnvironment(FVertexFactoryShaderPermutationParameters const& parameters,
                                     FShaderCompilerEnvironment& environment) {
        FLocalVertexFactory::ModifyCompilationEnvironment(parameters, environment);
        environment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
        environment.SetDefine(TEXT("USE_INSTANCE_CULLING"), TEXT("0"));
        environment.SetDefine(TEXT("USE_INSTANCING"), TEXT("1"));
        environment.SetDefine(TEXT("IS_INSTANCED_STATIC_MESH_VF"), TEXT("1"));
        environment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), TEXT("0"));
    }

    virtual void InitRHI(FRHICommandListBase& rhi_command_list) override {
        check(HasValidFeatureLevel());
        check(instance_buffer_ != nullptr);

        FVertexDeclarationElementList elements;
        GetVertexElements(GetFeatureLevel(),
                          EVertexInputStreamType::Default,
                          false,
                          Data,
                          elements,
                          Streams,
                          ColorStreamIndex);

        auto const instance_stride = sizeof(FSandboxISMCRenderInstance);
        auto const add_instance_element = [&](uint32 const offset, uint8 const attribute_index) {
            elements.Add(
                AccessStreamComponent(FVertexStreamComponent{instance_buffer_,
                                                             offset,
                                                             instance_stride,
                                                             VET_Float4,
                                                             EVertexStreamUsage::Instancing},
                                      attribute_index,
                                      Streams));
        };
        add_instance_element(offsetof(FSandboxISMCRenderInstance, origin), 8);
        add_instance_element(offsetof(FSandboxISMCRenderInstance, transform_row_0), 9);
        add_instance_element(offsetof(FSandboxISMCRenderInstance, transform_row_1), 10);
        add_instance_element(offsetof(FSandboxISMCRenderInstance, transform_row_2), 11);

        FVertexStreamComponent const null_lightmap{
            &GNullVertexBuffer, 0, 0, VET_Float4, EVertexStreamUsage::Instancing};
        elements.Add(AccessStreamComponent(null_lightmap, 12, Streams));

        InitDeclaration(elements);
        UniformBuffer = CreateLocalVFUniformBuffer(this, Data.LODLightmapDataIndex, nullptr, 0, 0);
    }
  private:
    FSandboxISMCInstanceBuffer const* instance_buffer_{nullptr};
};

class FSandboxISMCVertexFactoryShaderParameters final
    : public FLocalVertexFactoryShaderParametersBase {
    DECLARE_TYPE_LAYOUT(FSandboxISMCVertexFactoryShaderParameters, NonVirtual);
  public:
    void Bind(FShaderParameterMap const& parameter_map) {
        FLocalVertexFactoryShaderParametersBase::Bind(parameter_map);
        instancing_offset_.Bind(parameter_map, TEXT("InstancingOffset"));
        instance_offset_.Bind(parameter_map, TEXT("InstanceOffset"));
    }

    void GetElementShaderBindings(FSceneInterface const* scene,
                                  FSceneView const* view,
                                  FMeshMaterialShader const* shader,
                                  EVertexInputStreamType input_stream_type,
                                  ERHIFeatureLevel::Type feature_level,
                                  FVertexFactory const* vertex_factory,
                                  FMeshBatchElement const& batch_element,
                                  FMeshDrawSingleShaderBindings& shader_bindings,
                                  FVertexInputStreamArray& vertex_streams) const {
        auto const* local_vertex_factory =
            static_cast<FSandboxISMCVertexFactory const*>(vertex_factory);
        GetElementShaderBindingsBase(scene,
                                     view,
                                     shader,
                                     input_stream_type,
                                     feature_level,
                                     vertex_factory,
                                     batch_element,
                                     local_vertex_factory->GetUniformBuffer(),
                                     shader_bindings,
                                     vertex_streams);

        shader_bindings.Add(instancing_offset_, FVector4f::Zero());
        shader_bindings.Add(instance_offset_, batch_element.UserIndex);
    }
  private:
    LAYOUT_FIELD(FShaderParameter, instancing_offset_);
    LAYOUT_FIELD(FShaderParameter, instance_offset_);
};

IMPLEMENT_TYPE_LAYOUT(FSandboxISMCVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSandboxISMCVertexFactory,
                                        SF_Vertex,
                                        FSandboxISMCVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FSandboxISMCVertexFactory,
                              "/Engine/Private/LocalVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials |
                                  EVertexFactoryFlags::SupportsDynamicLighting |
                                  EVertexFactoryFlags::DoesNotSupportNullPixelShader);

class FSandboxISMCSceneProxy final : public FPrimitiveSceneProxy {
  public:
    FSandboxISMCSceneProxy(USandboxISMCComponent const* component,
                           TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> initial_update,
                           TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics)
        : FPrimitiveSceneProxy{component}
        , static_mesh_{component->get_static_mesh()}
        , render_data_{static_mesh_->GetRenderData()}
        , instance_buffer_{MoveTemp(initial_update), MoveTemp(metrics)}
        , vertex_factory_{GetScene().GetFeatureLevel(), &instance_buffer_}
        , material_relevance_{component->GetMaterialRelevance(GetScene().GetShaderPlatform())} {
        check(render_data_ != nullptr);
        check(!render_data_->LODResources.IsEmpty());

        auto const& lod = render_data_->LODResources[0];
        FLocalVertexFactory::FDataType vertex_data;
        lod.VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&vertex_factory_,
                                                                        vertex_data);
        lod.VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&vertex_factory_,
                                                                         vertex_data);
        lod.VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&vertex_factory_,
                                                                                vertex_data);

        if (lod.bHasColorVertexData) {
            lod.VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&vertex_factory_,
                                                                      vertex_data);
        } else {
            FColorVertexBuffer::BindDefaultColorVertexBuffer(
                &vertex_factory_,
                vertex_data,
                FColorVertexBuffer::NullBindStride::ZeroForDefaultBufferBind);
        }

        if (lod.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() > 0) {
            lod.VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(
                &vertex_factory_, vertex_data, 0);
        }

#if WITH_EDITORONLY_DATA
        vertex_data.StaticMesh = static_mesh_;
#endif
        vertex_factory_.set_static_mesh_data(vertex_data);

        auto const section_count = lod.Sections.Num();
        materials_.Reserve(section_count);
        for (auto section_index = 0; section_index < section_count; ++section_index) {
            auto const material_index = lod.Sections[section_index].MaterialIndex;
            auto* material = component->GetMaterial(material_index);
            if (material == nullptr ||
                !material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes)) {
                material = UMaterial::GetDefaultMaterial(MD_Surface);
            }
            materials_.Add(material);
        }

        instance_count_ = instance_buffer_.get_initial_instance_count();
        BeginInitResource(&instance_buffer_);
        BeginInitResource(&vertex_factory_);
    }

    virtual ~FSandboxISMCSceneProxy() override {
        vertex_factory_.ReleaseResource();
        instance_buffer_.ReleaseResource();
    }

    virtual SIZE_T GetTypeHash() const override {
        static size_t unique_pointer;
        return reinterpret_cast<SIZE_T>(&unique_pointer);
    }

    virtual void CreateRenderThreadResources(FRHICommandListBase& rhi_command_list) override {
        FPrimitiveSceneProxy::CreateRenderThreadResources(rhi_command_list);

        FInstancedStaticMeshVFLooseUniformShaderParameters parameters;
        parameters.InstanceDitherParameters = MakeNullLocalVertexFactoryInstanceDitherParameters();
        parameters.InstancingFadeOutParams = FVector4f{0.0f, 0.0f, 1.0f, 1.0f};
        loose_uniform_buffer_ =
            FInstancedStaticMeshVFLooseUniformShaderParametersRef::CreateUniformBufferImmediate(
                parameters, UniformBuffer_MultiFrame);
    }

    virtual void DestroyRenderThreadResources() override {
        loose_uniform_buffer_.SafeRelease();
        FPrimitiveSceneProxy::DestroyRenderThreadResources();
    }

    void update_instances_render_thread(
        FRHICommandListBase& rhi_command_list,
        TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> const& update) {
        check(IsInRenderingThread());
        instance_count_ = update->instance_count;
        instance_buffer_.upload(rhi_command_list, *update);
    }

    virtual void GetDynamicMeshElements(TArray<FSceneView const*> const& views,
                                        FSceneViewFamily const& view_family,
                                        uint32 visibility_map,
                                        FMeshElementCollector& collector) const override {
        if (instance_count_ == 0 || !loose_uniform_buffer_.IsValid()) {
            return;
        }

        auto const& lod = render_data_->LODResources[0];
        auto const view_count = views.Num();
        for (auto view_index = 0; view_index < view_count; ++view_index) {
            if ((visibility_map & (1u << view_index)) == 0) {
                continue;
            }

            auto const section_count = lod.Sections.Num();
            for (auto section_index = 0; section_index < section_count; ++section_index) {
                auto const& section = lod.Sections[section_index];
                auto& mesh = collector.AllocateMesh();
                auto& batch_element = mesh.Elements[0];

                mesh.VertexFactory = &vertex_factory_;
                mesh.MaterialRenderProxy = materials_[section_index]->GetRenderProxy();
                mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                mesh.Type = PT_TriangleList;
                mesh.DepthPriorityGroup = SDPG_World;
                mesh.CastShadow = CastsDynamicShadow();
                mesh.bUseForMaterial = true;
                mesh.bUseForDepthPass = true;
                mesh.bCanApplyViewModeOverrides = true;

                batch_element.IndexBuffer = &lod.IndexBuffer;
                batch_element.FirstIndex = section.FirstIndex;
                batch_element.NumPrimitives = section.NumTriangles;
                batch_element.MinVertexIndex = section.MinVertexIndex;
                batch_element.MaxVertexIndex = section.MaxVertexIndex;
                batch_element.NumInstances = instance_count_;
                batch_element.UserIndex = 0;
                batch_element.LooseParametersUniformBuffer = loose_uniform_buffer_;
                batch_element.PrimitiveIdMode = PrimID_ForceZero;

                auto& primitive_uniform_buffer =
                    collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
                FPrimitiveUniformShaderParametersBuilder builder;
                BuildUniformShaderParameters(builder);
                primitive_uniform_buffer.Set(collector.GetRHICommandList(), builder);
                batch_element.PrimitiveUniformBufferResource =
                    &primitive_uniform_buffer.UniformBuffer;

                collector.AddMesh(view_index, mesh);
            }
        }
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(FSceneView const* view) const override {
        FPrimitiveViewRelevance relevance;
        relevance.bDrawRelevance = IsShown(view);
        relevance.bShadowRelevance = IsShadowCast(view);
        relevance.bDynamicRelevance = true;
        relevance.bRenderInMainPass = ShouldRenderInMainPass();
        relevance.bUsesLightingChannels =
            GetLightingChannelMask() != GetDefaultLightingChannelMask();
        relevance.bRenderCustomDepth = ShouldRenderCustomDepth();
        relevance.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
        material_relevance_.SetPrimitiveViewRelevance(relevance);
        relevance.bVelocityRelevance = false;
        return relevance;
    }

    virtual bool CanBeOccluded() const override { return !material_relevance_.bDisableDepthTest; }

    virtual uint32 GetMemoryFootprint() const override {
        return sizeof(*this) + GetAllocatedSize();
    }
  private:
    uint32 GetAllocatedSize() const {
        return FPrimitiveSceneProxy::GetAllocatedSize() + materials_.GetAllocatedSize();
    }

    UStaticMesh* static_mesh_{nullptr};
    FStaticMeshRenderData* render_data_{nullptr};
    FSandboxISMCInstanceBuffer instance_buffer_;
    FSandboxISMCVertexFactory vertex_factory_;
    TArray<UMaterialInterface*> materials_;
    FMaterialRelevance material_relevance_;
    FInstancedStaticMeshVFLooseUniformShaderParametersRef loose_uniform_buffer_;
    uint32 instance_count_{0};
};
}

USandboxISMCComponent::USandboxISMCComponent()
    : metrics_{MakeShared<FSandboxISMCMetricsState, ESPMode::ThreadSafe>()} {
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    CanCharacterStepUpOn = ECB_No;
    bVisibleInRayTracing = false;
}

auto USandboxISMCComponent::set_static_mesh(UStaticMesh& mesh) -> void {
    if (static_mesh_ == &mesh) {
        return;
    }

    static_mesh_ = &mesh;
    auto const mesh_bounds{mesh.GetBounds()};
    mesh_bounds_origin_ = FVector3f{mesh_bounds.Origin};
    mesh_bounds_radius_ = static_cast<float>(mesh_bounds.SphereRadius);
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    mark_all_instances_dirty();
    commit_instance_updates();
    MarkRenderStateDirty();
}

auto USandboxISMCComponent::clear_static_mesh() -> void {
    if (static_mesh_ == nullptr) {
        return;
    }

    static_mesh_ = nullptr;
    mesh_bounds_origin_ = FVector3f::ZeroVector;
    mesh_bounds_radius_ = 0.0f;
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    mark_all_instances_dirty();
    commit_instance_updates();
    MarkRenderStateDirty();
}

auto USandboxISMCComponent::get_static_mesh() const -> UStaticMesh* {
    return static_mesh_;
}

auto USandboxISMCComponent::reserve_instances(int32 capacity) -> void {
    positions_.Reserve(capacity);
    rotations_.Reserve(capacity);
    scales_.Reserve(capacity);
}

auto USandboxISMCComponent::add_instance(FVector3f position, FQuat4f rotation, FVector3f scale)
    -> int32 {
    auto const instance_index = positions_.Num();
    positions_.Add(position);
    rotations_.Add(rotation);
    scales_.Add(scale);
    mark_instance_range_dirty(instance_index, 1);
    instance_count_changed_ = true;
    return instance_index;
}

auto USandboxISMCComponent::set_instance_transform(int32 instance_index,
                                                   FVector3f position,
                                                   FQuat4f rotation,
                                                   FVector3f scale) -> bool {
    if (!positions_.IsValidIndex(instance_index)) {
        UE_LOG(
            LogSandboxISMC, Warning, TEXT("Invalid SandboxISMC instance index %d"), instance_index);
        return false;
    }

    positions_[instance_index] = position;
    rotations_[instance_index] = rotation;
    scales_[instance_index] = scale;
    mark_instance_range_dirty(instance_index, 1);
    return true;
}

auto USandboxISMCComponent::remove_instance_swap(int32 instance_index) -> FSandboxISMCRemoveResult {
    if (!positions_.IsValidIndex(instance_index)) {
        UE_LOG(
            LogSandboxISMC, Warning, TEXT("Invalid SandboxISMC instance index %d"), instance_index);
        return {};
    }

    auto const previous_last_index = positions_.Num() - 1;
    positions_.RemoveAtSwap(instance_index, EAllowShrinking::No);
    rotations_.RemoveAtSwap(instance_index, EAllowShrinking::No);
    scales_.RemoveAtSwap(instance_index, EAllowShrinking::No);
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    instance_count_changed_ = true;
    mark_all_instances_dirty();

    return {
        .removed = true,
        .moved_from_index =
            instance_index == previous_last_index ? INDEX_NONE : previous_last_index,
    };
}

auto USandboxISMCComponent::clear_instances() -> void {
    positions_.Reset();
    rotations_.Reset();
    scales_.Reset();
    dirty_ranges_.Reset();
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    instance_count_changed_ = true;
}

auto USandboxISMCComponent::get_instance_count() const -> int32 {
    return positions_.Num();
}

auto USandboxISMCComponent::positions() -> TArrayView<FVector3f> {
    mark_all_instances_dirty();
    return positions_;
}

auto USandboxISMCComponent::positions() const -> TConstArrayView<FVector3f> {
    return positions_;
}

auto USandboxISMCComponent::rotations() -> TArrayView<FQuat4f> {
    mark_all_instances_dirty();
    return rotations_;
}

auto USandboxISMCComponent::rotations() const -> TConstArrayView<FQuat4f> {
    return rotations_;
}

auto USandboxISMCComponent::scales() -> TArrayView<FVector3f> {
    mark_all_instances_dirty();
    return scales_;
}

auto USandboxISMCComponent::scales() const -> TConstArrayView<FVector3f> {
    return scales_;
}

auto USandboxISMCComponent::edit_positions(int32 first_index, int32 count)
    -> TArrayView<FVector3f> {
    if (first_index < 0 || count < 0 || first_index + count > positions_.Num()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("Invalid SandboxISMC position edit range [%d, %d)"),
               first_index,
               first_index + count);
        return {};
    }

    mark_instance_range_dirty(first_index, count);
    return MakeArrayView(positions_).Slice(first_index, count);
}

auto USandboxISMCComponent::edit_rotations(int32 first_index, int32 count) -> TArrayView<FQuat4f> {
    if (first_index < 0 || count < 0 || first_index + count > rotations_.Num()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("Invalid SandboxISMC rotation edit range [%d, %d)"),
               first_index,
               first_index + count);
        return {};
    }

    mark_instance_range_dirty(first_index, count);
    return MakeArrayView(rotations_).Slice(first_index, count);
}

auto USandboxISMCComponent::edit_scales(int32 first_index, int32 count) -> TArrayView<FVector3f> {
    if (first_index < 0 || count < 0 || first_index + count > scales_.Num()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("Invalid SandboxISMC scale edit range [%d, %d)"),
               first_index,
               first_index + count);
        return {};
    }

    mark_instance_range_dirty(first_index, count);
    return MakeArrayView(scales_).Slice(first_index, count);
}

auto USandboxISMCComponent::mark_instance_range_dirty(int32 first_index, int32 count) -> void {
    if (count == 0) {
        return;
    }
    if (first_index < 0 || count < 0 || first_index + count > positions_.Num()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("Invalid SandboxISMC dirty range [%d, %d)"),
               first_index,
               first_index + count);
        return;
    }

    auto merged_first{first_index};
    auto merged_end{first_index + count};
    auto range_index{0};
    while (range_index < dirty_ranges_.Num()) {
        auto const& range{dirty_ranges_[range_index]};
        auto const range_end{range.first_index + range.count};
        if (range_end < merged_first) {
            ++range_index;
            continue;
        }
        if (range.first_index > merged_end) {
            break;
        }

        merged_first = FMath::Min(merged_first, range.first_index);
        merged_end = FMath::Max(merged_end, range_end);
        dirty_ranges_.RemoveAt(range_index, EAllowShrinking::No);
    }

    dirty_ranges_.Insert({.first_index = merged_first, .count = merged_end - merged_first},
                         range_index);
}

auto USandboxISMCComponent::mark_all_instances_dirty() -> void {
    dirty_ranges_.Reset();
    if (!positions_.IsEmpty()) {
        dirty_ranges_.Add({.first_index = 0, .count = positions_.Num()});
    }
}

auto USandboxISMCComponent::calculate_instance_bounds(int32 instance_index) const -> FBox3f {
    if (static_mesh_ == nullptr) {
        return FBox3f{ForceInit};
    }

    auto const& scale{scales_[instance_index]};
    auto const center{positions_[instance_index] +
                      rotations_[instance_index].RotateVector(mesh_bounds_origin_ * scale)};
    auto const radius{mesh_bounds_radius_ * scale.GetAbsMax()};
    auto const extent{FVector3f{radius}};
    return FBox3f{center - extent, center + extent};
}

auto USandboxISMCComponent::rebuild_bounds_tree() -> void {
    auto const instance_count{positions_.Num()};
    if (instance_count == 0) {
        bounds_tree_.Reset();
        bounds_leaf_capacity_ = 0;
        bounds_tree_valid_ = true;
        update_local_bounds_from_tree();
        return;
    }

    bounds_leaf_capacity_ = FMath::RoundUpToPowerOfTwo(instance_count);
    bounds_tree_.Init(FBox3f{ForceInit}, bounds_leaf_capacity_ * 2);
    for (auto instance_index = 0; instance_index < instance_count; ++instance_index) {
        bounds_tree_[bounds_leaf_capacity_ + instance_index] =
            calculate_instance_bounds(instance_index);
    }
    for (auto node_index = bounds_leaf_capacity_ - 1; node_index > 0; --node_index) {
        auto combined{bounds_tree_[node_index * 2]};
        combined += bounds_tree_[node_index * 2 + 1];
        bounds_tree_[node_index] = combined;
    }
    bounds_tree_valid_ = true;
    update_local_bounds_from_tree();
}

auto USandboxISMCComponent::update_bounds_tree(TConstArrayView<FSandboxISMCDirtyRange> dirty_ranges)
    -> void {
    if (bounds_leaf_capacity_ == 0) {
        update_local_bounds_from_tree();
        return;
    }

    auto const updates_every_instance{dirty_ranges.Num() == 1 && dirty_ranges[0].first_index == 0 &&
                                      dirty_ranges[0].count == positions_.Num()};
    if (updates_every_instance) {
        FBox3f local_box{ForceInit};
        auto const instance_count{positions_.Num()};
        for (auto instance_index = 0; instance_index < instance_count; ++instance_index) {
            auto const instance_bounds{calculate_instance_bounds(instance_index)};
            local_box += instance_bounds;
        }
        local_bounds_ = local_box.IsValid != 0
                          ? FBoxSphereBounds{FBoxSphereBounds3f{local_box}}
                          : FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0};
        bounds_tree_valid_ = false;
        return;
    }

    if (!bounds_tree_valid_) {
        rebuild_bounds_tree();
        return;
    }

    for (auto const& range : dirty_ranges) {
        auto const range_end{range.first_index + range.count};
        for (auto instance_index = range.first_index; instance_index < range_end;
             ++instance_index) {
            bounds_tree_[bounds_leaf_capacity_ + instance_index] =
                calculate_instance_bounds(instance_index);
        }

        auto first_node{(bounds_leaf_capacity_ + range.first_index) / 2};
        auto last_node{(bounds_leaf_capacity_ + range_end - 1) / 2};
        while (first_node > 0) {
            for (auto node_index = first_node; node_index <= last_node; ++node_index) {
                auto combined{bounds_tree_[node_index * 2]};
                combined += bounds_tree_[node_index * 2 + 1];
                bounds_tree_[node_index] = combined;
            }
            first_node /= 2;
            last_node /= 2;
        }
    }
    update_local_bounds_from_tree();
}

auto USandboxISMCComponent::update_local_bounds_from_tree() -> void {
    auto const has_valid_root{bounds_tree_.Num() > 1 && bounds_tree_[1].IsValid != 0};
    local_bounds_ = has_valid_root
                      ? FBoxSphereBounds{FBoxSphereBounds3f{bounds_tree_[1]}}
                      : FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0};
}

auto USandboxISMCComponent::commit_instance_updates() -> void {
    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_PackAndBounds);
    SCOPE_CYCLE_COUNTER(STAT_SandboxISMCPrepare);
    auto const start_cycles = FPlatformTime::Cycles64();

    check(positions_.Num() == rotations_.Num());
    check(positions_.Num() == scales_.Num());

    auto const instance_count = positions_.Num();
    auto const has_changes{force_full_upload_ || instance_count_changed_ ||
                           !dirty_ranges_.IsEmpty()};
    if (!has_changes) {
        metrics_->instance_count.Store(instance_count);
        metrics_->prepare_cycles.Store(FPlatformTime::Cycles64() - start_cycles);
        metrics_->pack_cycles.Store(0);
        metrics_->bounds_cycles.Store(0);
        metrics_->upload_bytes.Store(0);
        metrics_->dirty_instance_count.Store(0);
        metrics_->dirty_range_count.Store(0);
        return;
    }

    auto const full_upload{force_full_upload_ || pending_render_update_.IsValid() ||
                           SceneProxy == nullptr || instance_count > submitted_buffer_capacity_};
    TArray<FSandboxISMCDirtyRange> upload_ranges;
    if (full_upload) {
        if (instance_count > 0) {
            upload_ranges.Add({.first_index = 0, .count = instance_count});
        }
    } else {
        upload_ranges = dirty_ranges_;
    }

    int32 dirty_instance_count{0};
    for (auto const& range : upload_ranges) {
        dirty_instance_count += range.count;
    }

    auto update = MakeShared<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe>();
    update->instance_count = instance_count;
    update->full_upload = full_upload;
    update->ranges.Reserve(upload_ranges.Num());
    update->instances.SetNumUninitialized(dirty_instance_count);

    auto const pack_start_cycles = FPlatformTime::Cycles64();
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_PackTransforms);
        int32 packed_instance_index{0};
        for (auto const& dirty_range : upload_ranges) {
            update->ranges.Add(
                {.first_instance = dirty_range.first_index, .count = dirty_range.count});
            for (auto range_index = 0; range_index < dirty_range.count; ++range_index) {
                auto const instance_index{dirty_range.first_index + range_index};
                auto const transform{FTransform3f{rotations_[instance_index],
                                                  positions_[instance_index],
                                                  scales_[instance_index]}};
                auto const matrix{transform.ToMatrixWithScale()};
                auto& packed{update->instances[packed_instance_index]};
                packed.origin = FVector4f{positions_[instance_index], 0.0f};
                packed.transform_row_0 =
                    FVector4f{matrix.M[0][0], matrix.M[0][1], matrix.M[0][2], 0.0f};
                packed.transform_row_1 =
                    FVector4f{matrix.M[1][0], matrix.M[1][1], matrix.M[1][2], 0.0f};
                packed.transform_row_2 =
                    FVector4f{matrix.M[2][0], matrix.M[2][1], matrix.M[2][2], 0.0f};
                ++packed_instance_index;
            }
        }
        check(packed_instance_index == update->instances.Num());
    }
    auto const pack_cycles = FPlatformTime::Cycles64() - pack_start_cycles;

    auto const bounds_start_cycles = FPlatformTime::Cycles64();
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_UpdateConservativeBounds);
        if (bounds_rebuild_required_ || instance_count > bounds_leaf_capacity_) {
            rebuild_bounds_tree();
        } else {
            update_bounds_tree(dirty_ranges_);
        }
    }
    auto const bounds_cycles = FPlatformTime::Cycles64() - bounds_start_cycles;
    pending_render_update_ = MoveTemp(update);

    auto const elapsed_cycles = FPlatformTime::Cycles64() - start_cycles;
    auto const upload_bytes{static_cast<uint64>(dirty_instance_count) *
                            sizeof(FSandboxISMCRenderInstance)};
    metrics_->instance_count.Store(instance_count);
    metrics_->prepare_cycles.Store(elapsed_cycles);
    metrics_->pack_cycles.Store(pack_cycles);
    metrics_->bounds_cycles.Store(bounds_cycles);
    metrics_->upload_bytes.Store(upload_bytes);
    metrics_->dirty_instance_count.Store(dirty_instance_count);
    metrics_->dirty_range_count.Store(upload_ranges.Num());
    SET_DWORD_STAT(STAT_SandboxISMCInstances, instance_count);
    SET_DWORD_STAT(STAT_SandboxISMCUploadBytes,
                   static_cast<uint32>(FMath::Min<uint64>(upload_bytes, MAX_uint32)));

    if (instance_count > submitted_buffer_capacity_) {
        submitted_buffer_capacity_ = FMath::RoundUpToPowerOfTwo(FMath::Max(instance_count, 1));
    }
    dirty_ranges_.Reset();
    force_full_upload_ = false;
    bounds_rebuild_required_ = false;
    instance_count_changed_ = false;

    MarkRenderTransformDirty();
    MarkRenderDynamicDataDirty();
    if (IsRegistered() && SceneProxy == nullptr && static_mesh_ != nullptr && instance_count > 0) {
        MarkRenderStateDirty();
    }
}

auto USandboxISMCComponent::get_update_metrics() const -> FSandboxISMCUpdateMetrics {
    return {
        .instance_count = metrics_->instance_count.Load(),
        .prepare_ms = FPlatformTime::ToMilliseconds64(metrics_->prepare_cycles.Load()),
        .pack_ms = FPlatformTime::ToMilliseconds64(metrics_->pack_cycles.Load()),
        .bounds_ms = FPlatformTime::ToMilliseconds64(metrics_->bounds_cycles.Load()),
        .submit_ms = FPlatformTime::ToMilliseconds64(metrics_->submit_cycles.Load()),
        .upload_ms = FPlatformTime::ToMilliseconds64(metrics_->upload_cycles.Load()),
        .upload_bytes = metrics_->upload_bytes.Load(),
        .dirty_instance_count = metrics_->dirty_instance_count.Load(),
        .dirty_range_count = metrics_->dirty_range_count.Load(),
    };
}

auto USandboxISMCComponent::CreateSceneProxy() -> FPrimitiveSceneProxy* {
    if (static_mesh_ == nullptr || positions_.IsEmpty()) {
        return nullptr;
    }

    auto* render_data = static_mesh_->GetRenderData();
    if (render_data == nullptr || render_data->LODResources.IsEmpty()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("Static mesh '%s' has no conventional LOD render data for SandboxISMC"),
               *GetNameSafe(static_mesh_));
        return nullptr;
    }

    if (!pending_render_update_.IsValid()) {
        UE_LOG(LogSandboxISMC,
               Warning,
               TEXT("SandboxISMC has CPU instances but no committed render snapshot"));
        return nullptr;
    }

    auto initial_update = pending_render_update_;
    pending_render_update_.Reset();
    return new FSandboxISMCSceneProxy{this, MoveTemp(initial_update), metrics_};
}

auto USandboxISMCComponent::GetNumMaterials() const -> int32 {
    auto const static_material_count =
        static_mesh_ != nullptr ? static_mesh_->GetStaticMaterials().Num() : 0;
    return FMath::Max(static_material_count, OverrideMaterials.Num());
}

auto USandboxISMCComponent::GetMaterial(int32 element_index) const -> UMaterialInterface* {
    if (auto* override_material = Super::GetMaterial(element_index)) {
        return override_material;
    }

    return static_mesh_ != nullptr ? static_mesh_->GetMaterial(element_index) : nullptr;
}

auto USandboxISMCComponent::CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds {
    return local_bounds_.TransformBy(local_to_world);
}

auto USandboxISMCComponent::SendRenderDynamicData_Concurrent() -> void {
    Super::SendRenderDynamicData_Concurrent();

    if (SceneProxy == nullptr || !pending_render_update_.IsValid()) {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_SubmitRenderUpdate);
    SCOPE_CYCLE_COUNTER(STAT_SandboxISMCSubmit);
    auto const start_cycles = FPlatformTime::Cycles64();
    auto* proxy = static_cast<FSandboxISMCSceneProxy*>(SceneProxy);
    auto update = pending_render_update_;
    pending_render_update_.Reset();

    ENQUEUE_RENDER_COMMAND(FSandboxISMCUpdateInstances)(
        [proxy, update = MoveTemp(update)](FRHICommandListImmediate& rhi_command_list) {
            proxy->update_instances_render_thread(rhi_command_list, update);
        });

    metrics_->submit_cycles.Store(FPlatformTime::Cycles64() - start_cycles);
}
