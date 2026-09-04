#pragma once

#include "SandboxISMCInstanceState.h"
#include "SandboxISMCUpdateMetrics.h"

#include "Components/MeshComponent.h"
#include "Containers/ArrayView.h"
#include "Templates/SharedPointer.h"

#include "SandboxISMCComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
struct FSandboxISMCMetricsState;

UCLASS(ClassGroup = (Rendering), meta = (DisplayName = "Sandbox ISMC"))
class SANDBOXISMC_API USandboxISMCComponent final : public UMeshComponent {
    GENERATED_BODY()
  public:
    USandboxISMCComponent();

    auto set_static_mesh(UStaticMesh& mesh) -> void;
    auto clear_static_mesh() -> void;
    auto get_static_mesh() const -> UStaticMesh*;

    auto reserve_instances(int32 capacity) -> void;
    auto add_instances(TConstArrayView<FVector3f> positions) -> int32;
    auto add_instances(TConstArrayView<FVector3f> positions, TConstArrayView<FQuat4f> rotations)
        -> int32;
    auto add_instances(ml::sandbox_ismc::InstanceDataConstView instances) -> int32;
    auto set_instance_transforms(int32 first_index,
                                 ml::sandbox_ismc::InstanceDataConstView instances) -> void;
    auto set_instance_transforms(TConstArrayView<int32> instance_indices,
                                 ml::sandbox_ismc::InstanceDataConstView instances) -> void;
    auto remove_instances_swap(TConstArrayView<int32> sorted_instance_indices) -> void;
    auto clear_instances() -> void;

    auto get_instance_count() const -> int32;
    auto instances() -> ml::sandbox_ismc::InstanceDataView;
    auto instances() const -> ml::sandbox_ismc::InstanceDataConstView;
    auto edit_instances(int32 first_index, int32 count) -> ml::sandbox_ismc::InstanceDataView;
    auto mark_instance_range_dirty(int32 first_index, int32 count) -> void;
    auto mark_all_instances_dirty() -> void;

    auto commit_instance_updates() -> void;
    auto get_update_metrics() const -> FSandboxISMCUpdateMetrics;

    virtual auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    virtual auto GetNumMaterials() const -> int32 override;
    virtual auto GetMaterial(int32 element_index) const -> UMaterialInterface* override;
    virtual auto CalcBounds(FTransform const& local_to_world) const -> FBoxSphereBounds override;
    virtual auto SendRenderDynamicData_Concurrent() -> void override;
  private:
    UPROPERTY(EditAnywhere, Category = "Mesh")
    TObjectPtr<UStaticMesh> static_mesh_;

    ml::sandbox_ismc::InstanceState instance_state_;
    TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> pending_render_update_;
    TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics_;
};
