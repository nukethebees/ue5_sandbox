#pragma once

#include "SandboxISMCDirtyRange.h"
#include "SandboxISMCInstanceData.h"
#include "SandboxISMCPreparedUpdate.h"

#include "Containers/ArrayView.h"
#include "Math/BoxSphereBounds.h"

namespace ml::sandbox_ismc {
class SANDBOXISMC_API InstanceState final {
  public:
    auto set_mesh_bounds(FVector3f origin, float radius) -> void;
    auto clear_mesh_bounds() -> void;

    auto reserve_instances(int32 capacity) -> void;
    auto add_instances(TConstArrayView<FVector3f> positions) -> int32;
    auto add_instances(TConstArrayView<FVector3f> positions, TConstArrayView<FQuat4f> rotations)
        -> int32;
    auto add_instances(InstanceDataConstView instances) -> int32;
    auto set_instance_transforms(int32 first_index, InstanceDataConstView instances) -> void;
    auto set_instance_transforms(TConstArrayView<int32> instance_indices,
                                 InstanceDataConstView instances) -> void;
    auto remove_instances_swap(TConstArrayView<int32> sorted_instance_indices) -> void;
    auto clear_instances() -> void;

    auto get_instance_count() const -> int32;
    auto instances() -> InstanceDataView;
    auto instances() const -> InstanceDataConstView;
    auto edit_instances(int32 first_index, int32 count) -> InstanceDataView;
    auto mark_instance_range_dirty(int32 first_index, int32 count) -> void;
    auto mark_all_instances_dirty() -> void;

    auto prepare_update(bool render_snapshot_invalid) -> FSandboxISMCPreparedUpdate;
    auto local_bounds() const -> FBoxSphereBounds const&;
  private:
    auto is_valid_instance_range(int32 first_index, int32 count) const -> bool;
    static auto is_valid_instance_range(int32 first_index, int32 count, int32 instance_count)
        -> bool;
    auto coalesce_dirty_ranges() -> void;

    auto calculate_instance_bounds(int32 instance_index) const -> FBox3f;
    auto rebuild_bounds_tree() -> void;
    auto update_bounds_tree(TConstArrayView<FSandboxISMCDirtyRange> dirty_ranges) -> void;
    auto update_local_bounds_from_tree() -> void;

    InstanceData instance_data_;
    TArray<FSandboxISMCDirtyRange> dirty_ranges_;
    TArray<FBox3f> bounds_tree_;

    FBoxSphereBounds local_bounds_{ForceInit};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    float mesh_bounds_radius_{0.0f};
    int32 bounds_leaf_capacity_{0};
    int32 submitted_buffer_capacity_{0};
    bool has_mesh_bounds_{false};
    bool force_full_upload_{true};
    bool bounds_rebuild_required_{true};
    bool bounds_tree_valid_{false};
    bool instance_count_changed_{false};
};
}
