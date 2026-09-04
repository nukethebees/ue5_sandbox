#pragma once

#include "SandboxISMCInstanceChunkWriter.h"
#include "SandboxISMCParallelism.h"
#include "SandboxISMCUpdateMetrics.h"

#include "Components/MeshComponent.h"
#include "Math/BoxSphereBounds.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

#include "SandboxISMCComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
struct FSandboxISMCMetricsState;
struct FSandboxISMCRenderUpdate;

UCLASS(ClassGroup = (Rendering), meta = (DisplayName = "Sandbox ISMC"))
class SANDBOXISMC_API USandboxISMCComponent final : public UMeshComponent {
    GENERATED_BODY()
  public:
    USandboxISMCComponent();

    auto set_static_mesh(UStaticMesh& mesh) -> void;
    auto clear_static_mesh() -> void;
    auto get_static_mesh() const -> UStaticMesh*;

    template <typename FillChunk>
    auto set_instances(int32 instance_count,
                       ESandboxISMCParallelism parallelism,
                       FillChunk&& fill_chunk) -> void {
        set_instances_internal(instance_count, parallelism, fill_chunk);
    }

    auto clear_instances() -> void;
    auto get_instance_count() const -> int32;
    auto get_update_metrics() const -> FSandboxISMCUpdateMetrics;

    virtual auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    virtual auto GetNumMaterials() const -> int32 override;
    virtual auto GetMaterial(int32 element_index) const -> UMaterialInterface* override;
    virtual auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
    virtual auto SendRenderDynamicData_Concurrent() -> void override;
  private:
    auto set_instances_internal(int32 instance_count,
                                ESandboxISMCParallelism parallelism,
                                TFunctionRef<void(FSandboxISMCInstanceChunkWriter&)> fill_chunk)
        -> void;

    UPROPERTY(EditAnywhere, Category = "Mesh")
    TObjectPtr<UStaticMesh> static_mesh_;

    TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> pending_render_update_;
    TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics_;
    FBoxSphereBounds local_bounds_{ForceInit};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    float mesh_bounds_radius_{0.0f};
    int32 instance_count_{0};
    bool has_mesh_bounds_{false};
};
