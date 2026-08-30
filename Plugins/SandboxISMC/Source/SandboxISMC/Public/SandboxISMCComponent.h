#pragma once

#include "Components/MeshComponent.h"
#include "Containers/ArrayView.h"
#include "Templates/SharedPointer.h"

#include "SandboxISMCComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
struct FSandboxISMCMetricsState;
struct FSandboxISMCRenderUpdate;

struct FSandboxISMCDirtyRange {
    int32 first_index{0};
    int32 count{0};
};

struct SANDBOXISMC_API FSandboxISMCRemoveResult {
    bool removed{false};
    int32 moved_from_index{INDEX_NONE};
};

struct SANDBOXISMC_API FSandboxISMCUpdateMetrics {
    int32 instance_count{0};
    double prepare_ms{0.0};
    double pack_ms{0.0};
    double bounds_ms{0.0};
    double submit_ms{0.0};
    double upload_ms{0.0};
    uint64 upload_bytes{0};
    int32 dirty_instance_count{0};
    int32 dirty_range_count{0};
};

UCLASS(ClassGroup = (Rendering), meta = (DisplayName = "Sandbox ISMC"))
class SANDBOXISMC_API USandboxISMCComponent final : public UMeshComponent {
    GENERATED_BODY()
  public:
    USandboxISMCComponent();

    void set_static_mesh(UStaticMesh* mesh);
    UStaticMesh* get_static_mesh() const;

    void reserve_instances(int32 capacity);
    int32 add_instance(FVector3f position,
                       FQuat4f rotation = FQuat4f::Identity,
                       FVector3f scale = FVector3f::OneVector);
    bool set_instance_transform(int32 instance_index,
                                FVector3f position,
                                FQuat4f rotation,
                                FVector3f scale);
    FSandboxISMCRemoveResult remove_instance_swap(int32 instance_index);
    void clear_instances();

    int32 get_instance_count() const;
    TArrayView<FVector3f> positions();
    TConstArrayView<FVector3f> positions() const;
    TArrayView<FQuat4f> rotations();
    TConstArrayView<FQuat4f> rotations() const;
    TArrayView<FVector3f> scales();
    TConstArrayView<FVector3f> scales() const;
    TArrayView<FVector3f> edit_positions(int32 first_index, int32 count);
    TArrayView<FQuat4f> edit_rotations(int32 first_index, int32 count);
    TArrayView<FVector3f> edit_scales(int32 first_index, int32 count);
    void mark_instance_range_dirty(int32 first_index, int32 count);
    void mark_all_instances_dirty();

    void commit_instance_updates();
    FSandboxISMCUpdateMetrics get_update_metrics() const;

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual int32 GetNumMaterials() const override;
    virtual UMaterialInterface* GetMaterial(int32 element_index) const override;
    virtual FBoxSphereBounds CalcBounds(FTransform const& local_to_world) const override;
    virtual void SendRenderDynamicData_Concurrent() override;
  private:
    FBox3f calculate_instance_bounds(int32 instance_index) const;
    void rebuild_bounds_tree();
    void update_bounds_tree(TConstArrayView<FSandboxISMCDirtyRange> dirty_ranges);
    void update_local_bounds_from_tree();

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
