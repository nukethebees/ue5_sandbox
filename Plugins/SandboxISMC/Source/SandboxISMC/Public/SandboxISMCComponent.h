#pragma once

#include "SandboxISMCDirtyRange.h"
#include "SandboxISMCRemoveResult.h"
#include "SandboxISMCUpdateMetrics.h"

#include "Components/MeshComponent.h"
#include "Containers/ArrayView.h"
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

    auto set_static_mesh(UStaticMesh* mesh) -> void;
    auto get_static_mesh() const -> UStaticMesh*;

    auto reserve_instances(int32 capacity) -> void;
    auto add_instance(FVector3f position,
                      FQuat4f rotation = FQuat4f::Identity,
                      FVector3f scale = FVector3f::OneVector) -> int32;
    auto set_instance_transform(int32 instance_index,
                                FVector3f position,
                                FQuat4f rotation,
                                FVector3f scale) -> bool;
    auto remove_instance_swap(int32 instance_index) -> FSandboxISMCRemoveResult;
    auto clear_instances() -> void;

    auto get_instance_count() const -> int32;
    auto positions() -> TArrayView<FVector3f>;
    auto positions() const -> TConstArrayView<FVector3f>;
    auto rotations() -> TArrayView<FQuat4f>;
    auto rotations() const -> TConstArrayView<FQuat4f>;
    auto scales() -> TArrayView<FVector3f>;
    auto scales() const -> TConstArrayView<FVector3f>;
    auto edit_positions(int32 first_index, int32 count) -> TArrayView<FVector3f>;
    auto edit_rotations(int32 first_index, int32 count) -> TArrayView<FQuat4f>;
    auto edit_scales(int32 first_index, int32 count) -> TArrayView<FVector3f>;
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
    auto calculate_instance_bounds(int32 instance_index) const -> FBox3f;
    auto rebuild_bounds_tree() -> void;
    auto update_bounds_tree(TConstArrayView<FSandboxISMCDirtyRange> dirty_ranges) -> void;
    auto update_local_bounds_from_tree() -> void;

    UPROPERTY(EditAnywhere, Category = "Mesh")
    TObjectPtr<UStaticMesh> static_mesh_;

    TArray<FVector3f> positions_;
    TArray<FQuat4f> rotations_;
    TArray<FVector3f> scales_;
    TArray<FSandboxISMCDirtyRange> dirty_ranges_;
    TArray<FBox3f> bounds_tree_;

    FBoxSphereBounds local_bounds_{ForceInit};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    float mesh_bounds_radius_{0.0f};
    TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> pending_render_update_;
    TSharedPtr<FSandboxISMCMetricsState, ESPMode::ThreadSafe> metrics_;
    int32 bounds_leaf_capacity_{0};
    int32 submitted_buffer_capacity_{0};
    bool force_full_upload_{true};
    bool bounds_rebuild_required_{true};
    bool bounds_tree_valid_{false};
    bool instance_count_changed_{false};
};
