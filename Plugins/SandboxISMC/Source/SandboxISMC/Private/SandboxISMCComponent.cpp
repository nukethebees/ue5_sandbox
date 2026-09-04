#include "SandboxISMCComponent.h"

#include "SandboxISMCRenderUpdate.h"

#include "Async/ParallelFor.h"
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
DECLARE_CYCLE_STAT(TEXT("Build instance snapshot"), STAT_SandboxISMCBuild, STATGROUP_SandboxISMC);
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
    int32 instance_count{0};
    uint64 build_cycles{0};
    uint64 submit_cycles{0};
    uint64 upload_bytes{0};
    TAtomic<uint64> upload_cycles{0};
};

namespace {
constexpr int32 instance_chunk_size{1024};
constexpr int32 parallel_instance_threshold{4096};

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
        auto const initial_count = initial_update_.IsValid() ? initial_update_->instances.Num() : 0;
        allocate(rhi_command_list, FMath::Max(initial_count, 1));

        if (initial_update_.IsValid()) {
            upload(rhi_command_list, *initial_update_);
            initial_update_.Reset();
        }
    }

    int32 get_initial_instance_count() const {
        return initial_update_.IsValid() ? initial_update_->instances.Num() : 0;
    }

    void upload(FRHICommandListBase& rhi_command_list, FSandboxISMCRenderUpdate const& update) {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_RenderThreadUpload);
        SCOPE_CYCLE_COUNTER(STAT_SandboxISMCUpload);
        auto const start_cycles = FPlatformTime::Cycles64();
        auto const instance_count = update.instances.Num();

        if (instance_count > capacity_) {
            allocate(rhi_command_list, instance_count);
        }

        auto const byte_count{static_cast<uint64>(instance_count) *
                              sizeof(FSandboxISMCRenderInstance)};
        if (byte_count > 0) {
            auto* destination = rhi_command_list.LockBuffer(
                VertexBufferRHI, 0, static_cast<uint32>(byte_count), RLM_WriteOnly);
            FMemory::Memcpy(destination, update.instances.GetData(), byte_count);
            rhi_command_list.UnlockBuffer(VertexBufferRHI);
        }

        auto const elapsed_cycles = FPlatformTime::Cycles64() - start_cycles;
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadMs,
                                 FPlatformTime::ToMilliseconds64(elapsed_cycles));
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadBytes,
                                 static_cast<int64>(byte_count));
        TRACE_COUNTER_SET_ALWAYS(SandboxISMCRenderThreadUploadInstances, instance_count);
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
        instance_count_ = update->instances.Num();
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
    has_mesh_bounds_ = true;
    clear_instances();
    MarkRenderStateDirty();
}

auto USandboxISMCComponent::clear_static_mesh() -> void {
    if (static_mesh_ == nullptr) {
        return;
    }

    static_mesh_ = nullptr;
    mesh_bounds_origin_ = FVector3f::ZeroVector;
    mesh_bounds_radius_ = 0.0f;
    has_mesh_bounds_ = false;
    clear_instances();
    MarkRenderStateDirty();
}

auto USandboxISMCComponent::get_static_mesh() const -> UStaticMesh* {
    return static_mesh_;
}

auto USandboxISMCComponent::clear_instances() -> void {
    set_instances_internal(
        0, ESandboxISMCParallelism::Sequential, [](FSandboxISMCInstanceChunkWriter&) {});
}

auto USandboxISMCComponent::get_instance_count() const -> int32 {
    return instance_count_;
}

auto USandboxISMCComponent::set_instances_internal(
    int32 instance_count,
    ESandboxISMCParallelism parallelism,
    TFunctionRef<void(FSandboxISMCInstanceChunkWriter&)> fill_chunk) -> void {
    checkf(instance_count >= 0, TEXT("SandboxISMC instance count must not be negative"));
    TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_BuildSnapshot);
    SCOPE_CYCLE_COUNTER(STAT_SandboxISMCBuild);
    auto const start_cycles{FPlatformTime::Cycles64()};

    auto update{MakeShared<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe>()};
    update->instances.SetNumUninitialized(instance_count);

    auto const chunk_count{FMath::DivideAndRoundUp(instance_count, instance_chunk_size)};
    TArray<FBox3f> chunk_bounds;
    chunk_bounds.SetNum(chunk_count);

    auto const build_chunk{[&](int32 chunk_index) {
        auto const first_index{chunk_index * instance_chunk_size};
        auto const count{FMath::Min(instance_chunk_size, instance_count - first_index)};
        auto instances{MakeArrayView(update->instances).Slice(first_index, count)};
        FSandboxISMCInstanceChunkWriter writer{
            instances, first_index, mesh_bounds_origin_, mesh_bounds_radius_, has_mesh_bounds_};
        fill_chunk(writer);
        chunk_bounds[chunk_index] = writer.bounds();
    }};

    auto const run_parallel{parallelism == ESandboxISMCParallelism::Parallel ||
                            (parallelism == ESandboxISMCParallelism::Auto &&
                             instance_count >= parallel_instance_threshold)};
    if (run_parallel) {
        ParallelFor(chunk_count, build_chunk);
    } else {
        for (auto chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
            build_chunk(chunk_index);
        }
    }

    FBox3f local_box{ForceInit};
    for (auto const& bounds : chunk_bounds) {
        local_box += bounds;
    }
    local_bounds_ = local_box.IsValid != 0
                      ? FBoxSphereBounds{FBoxSphereBounds3f{local_box}}
                      : FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0};

    pending_render_update_ = MoveTemp(update);
    instance_count_ = instance_count;

    auto const elapsed_cycles{FPlatformTime::Cycles64() - start_cycles};
    auto const upload_bytes{static_cast<uint64>(instance_count) *
                            sizeof(FSandboxISMCRenderInstance)};
    metrics_->instance_count = instance_count;
    metrics_->build_cycles = elapsed_cycles;
    metrics_->upload_bytes = upload_bytes;
    SET_DWORD_STAT(STAT_SandboxISMCInstances, instance_count);
    SET_DWORD_STAT(STAT_SandboxISMCUploadBytes,
                   static_cast<uint32>(FMath::Min<uint64>(upload_bytes, MAX_uint32)));

    MarkRenderTransformDirty();
    MarkRenderDynamicDataDirty();
    if (IsRegistered() && SceneProxy == nullptr && static_mesh_ != nullptr && instance_count > 0) {
        MarkRenderStateDirty();
    }
}

auto USandboxISMCComponent::get_update_metrics() const -> FSandboxISMCUpdateMetrics {
    return {
        .instance_count = metrics_->instance_count,
        .build_ms = FPlatformTime::ToMilliseconds64(metrics_->build_cycles),
        .submit_ms = FPlatformTime::ToMilliseconds64(metrics_->submit_cycles),
        .upload_ms = FPlatformTime::ToMilliseconds64(metrics_->upload_cycles.Load()),
        .upload_bytes = metrics_->upload_bytes,
    };
}

auto USandboxISMCComponent::CreateSceneProxy() -> FPrimitiveSceneProxy* {
    if (static_mesh_ == nullptr || instance_count_ == 0) {
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

    metrics_->submit_cycles = FPlatformTime::Cycles64() - start_cycles;
}
