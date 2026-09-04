#include "SandboxISMCInstanceState.h"

#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxISMCState, Log, All);

namespace ml::sandbox_ismc {
auto InstanceState::set_mesh_bounds(FVector3f origin, float radius) -> void {
    mesh_bounds_origin_ = origin;
    mesh_bounds_radius_ = radius;
    has_mesh_bounds_ = true;
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    mark_all_instances_dirty();
}

auto InstanceState::clear_mesh_bounds() -> void {
    mesh_bounds_origin_ = FVector3f::ZeroVector;
    mesh_bounds_radius_ = 0.0f;
    has_mesh_bounds_ = false;
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    mark_all_instances_dirty();
}

auto InstanceState::reserve_instances(int32 capacity) -> void {
    instance_data_.reserve(capacity);
}

auto InstanceState::add_instances(TConstArrayView<FVector3f> positions) -> int32 {
    auto const count{positions.Num()};
    if (count == 0) {
        return INDEX_NONE;
    }

    auto const first_index{instance_data_.num()};
    instance_data_.add_uninitialised(count);
    auto const added_instances{instance_data_.right(count)};
    for (auto index = 0; index < count; ++index) {
        added_instances.positions[index] = positions[index];
        added_instances.rotations[index] = FQuat4f::Identity;
        added_instances.scales[index] = FVector3f::OneVector;
    }

    mark_instance_range_dirty(first_index, count);
    instance_count_changed_ = true;
    return first_index;
}

auto InstanceState::add_instances(TConstArrayView<FVector3f> positions,
                                  TConstArrayView<FQuat4f> rotations) -> int32 {
    checkf(positions.Num() == rotations.Num(),
           TEXT("SandboxISMC add views must have equal lengths"));

    auto const count{positions.Num()};
    if (count == 0) {
        return INDEX_NONE;
    }

    auto const first_index{instance_data_.num()};
    instance_data_.add_uninitialised(count);
    auto const added_instances{instance_data_.right(count)};
    for (auto index = 0; index < count; ++index) {
        added_instances.positions[index] = positions[index];
        added_instances.rotations[index] = rotations[index];
        added_instances.scales[index] = FVector3f::OneVector;
    }

    mark_instance_range_dirty(first_index, count);
    instance_count_changed_ = true;
    return first_index;
}

auto InstanceState::add_instances(InstanceDataConstView instances) -> int32 {
    instances.validate_array_sizes();

    auto const count{instances.num()};
    if (count == 0) {
        return INDEX_NONE;
    }

    auto const first_index{instance_data_.num()};
    instance_data_.append_from(instances);
    mark_instance_range_dirty(first_index, count);
    instance_count_changed_ = true;
    return first_index;
}

auto InstanceState::set_instance_transforms(int32 first_index, InstanceDataConstView instances)
    -> void {
    instances.validate_array_sizes();
    auto const count{instances.num()};
    checkf(is_valid_instance_range(first_index, count),
           TEXT("Invalid SandboxISMC transform range: first index %d, count %d"),
           first_index,
           count);

    instance_data_.copy_elements(first_index, instances, 0, count);
    mark_instance_range_dirty(first_index, count);
}

auto InstanceState::set_instance_transforms(TConstArrayView<int32> instance_indices,
                                            InstanceDataConstView instances) -> void {
    instances.validate_array_sizes();
    checkf(instance_indices.Num() == instances.num(),
           TEXT("SandboxISMC scattered transform views must have equal lengths"));

    for (auto const instance_index : instance_indices) {
        checkf(instance_data_.positions.IsValidIndex(instance_index),
               TEXT("Invalid SandboxISMC instance index %d"),
               instance_index);
    }

    auto const count{instance_indices.Num()};
    dirty_ranges_.Reserve(dirty_ranges_.Num() + count);
    for (auto index = 0; index < count; ++index) {
        auto const instance_index{instance_indices[index]};
        instance_data_.copy_element(instance_index, instances, index);
        mark_instance_range_dirty(instance_index, 1);
    }
}

auto InstanceState::remove_instances_swap(TConstArrayView<int32> sorted_instance_indices) -> void {
    auto previous_index{instance_data_.num()};
    for (auto const instance_index : sorted_instance_indices) {
        checkf(instance_index >= 0 && instance_index < previous_index,
               TEXT("SandboxISMC removal indices must be valid, unique, and sorted descending"));
        previous_index = instance_index;
    }

    if (sorted_instance_indices.IsEmpty()) {
        return;
    }

    for (auto const instance_index : sorted_instance_indices) {
        instance_data_.remove_at_swap(instance_index, 1, EAllowShrinking::No);
    }
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    instance_count_changed_ = true;
    mark_all_instances_dirty();
}

auto InstanceState::clear_instances() -> void {
    instance_data_.reset();
    dirty_ranges_.Reset();
    force_full_upload_ = true;
    bounds_rebuild_required_ = true;
    instance_count_changed_ = true;
}

auto InstanceState::get_instance_count() const -> int32 {
    return instance_data_.num();
}

auto InstanceState::instances() -> InstanceDataView {
    mark_all_instances_dirty();
    return instance_data_.get_view();
}

auto InstanceState::instances() const -> InstanceDataConstView {
    return instance_data_.get_const_view();
}

auto InstanceState::edit_instances(int32 first_index, int32 count) -> InstanceDataView {
    checkf(is_valid_instance_range(first_index, count),
           TEXT("Invalid SandboxISMC instance edit range: first index %d, count %d"),
           first_index,
           count);
    mark_instance_range_dirty(first_index, count);
    return instance_data_.get_view(first_index, count);
}

auto InstanceState::mark_instance_range_dirty(int32 first_index, int32 count) -> void {
    if (count == 0) {
        return;
    }
    if (!is_valid_instance_range(first_index, count)) {
        UE_LOG(LogSandboxISMCState,
               Warning,
               TEXT("Invalid SandboxISMC dirty range: first index %d, count %d"),
               first_index,
               count);
        return;
    }

    dirty_ranges_.Add({.first_index = first_index, .count = count});
}

auto InstanceState::mark_all_instances_dirty() -> void {
    dirty_ranges_.Reset();
    if (!instance_data_.is_empty()) {
        dirty_ranges_.Add({.first_index = 0, .count = instance_data_.num()});
    }
}

auto InstanceState::prepare_update(bool render_snapshot_invalid) -> FSandboxISMCPreparedUpdate {
    instance_data_.validate_array_sizes();

    auto const instance_count{instance_data_.num()};
    auto const has_changes{force_full_upload_ || instance_count_changed_ ||
                           !dirty_ranges_.IsEmpty()};
    if (!has_changes) {
        return {};
    }

    coalesce_dirty_ranges();

    auto const full_upload{force_full_upload_ || render_snapshot_invalid ||
                           instance_count > submitted_buffer_capacity_};
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

    auto update{MakeShared<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe>()};
    update->instance_count = instance_count;
    update->full_upload = full_upload;
    update->ranges.Reserve(upload_ranges.Num());
    update->instances.SetNumUninitialized(dirty_instance_count);

    auto const pack_start_cycles{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_PackTransforms);
        int32 packed_instance_index{0};
        for (auto const& dirty_range : upload_ranges) {
            update->ranges.Add(
                {.first_instance = dirty_range.first_index, .count = dirty_range.count});
            for (auto range_index = 0; range_index < dirty_range.count; ++range_index) {
                auto const instance_index{dirty_range.first_index + range_index};
                auto const transform{FTransform3f{instance_data_.rotations[instance_index],
                                                  instance_data_.positions[instance_index],
                                                  instance_data_.scales[instance_index]}};
                auto const matrix{transform.ToMatrixWithScale()};
                auto& packed{update->instances[packed_instance_index]};
                packed.origin = FVector4f{instance_data_.positions[instance_index], 0.0f};
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
    auto const pack_cycles{FPlatformTime::Cycles64() - pack_start_cycles};

    auto const bounds_start_cycles{FPlatformTime::Cycles64()};
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(SandboxISMC_Custom_UpdateConservativeBounds);
        if (bounds_rebuild_required_ || instance_count > bounds_leaf_capacity_) {
            rebuild_bounds_tree();
        } else {
            update_bounds_tree(dirty_ranges_);
        }
    }
    auto const bounds_cycles{FPlatformTime::Cycles64() - bounds_start_cycles};

    if (instance_count > submitted_buffer_capacity_) {
        submitted_buffer_capacity_ = FMath::RoundUpToPowerOfTwo(FMath::Max(instance_count, 1));
    }
    dirty_ranges_.Reset();
    force_full_upload_ = false;
    bounds_rebuild_required_ = false;
    instance_count_changed_ = false;

    return {
        .render_update = MoveTemp(update),
        .pack_cycles = pack_cycles,
        .bounds_cycles = bounds_cycles,
        .dirty_instance_count = dirty_instance_count,
        .dirty_range_count = upload_ranges.Num(),
    };
}

auto InstanceState::local_bounds() const -> FBoxSphereBounds const& {
    return local_bounds_;
}

auto InstanceState::is_valid_instance_range(int32 first_index, int32 count) const -> bool {
    return is_valid_instance_range(first_index, count, instance_data_.num());
}

auto InstanceState::is_valid_instance_range(int32 first_index, int32 count, int32 instance_count)
    -> bool {
    return first_index >= 0 && count >= 0 && first_index <= instance_count &&
           count <= instance_count - first_index;
}

auto InstanceState::coalesce_dirty_ranges() -> void {
    auto const range_count{dirty_ranges_.Num()};
    if (range_count < 2) {
        return;
    }

    dirty_ranges_.Sort([](FSandboxISMCDirtyRange const& lhs, FSandboxISMCDirtyRange const& rhs) {
        return lhs.first_index < rhs.first_index;
    });

    auto write_index{0};
    for (auto read_index = 1; read_index < range_count; ++read_index) {
        auto& merged_range{dirty_ranges_[write_index]};
        auto const& range{dirty_ranges_[read_index]};
        auto const merged_end{merged_range.first_index + merged_range.count};
        if (range.first_index > merged_end) {
            ++write_index;
            dirty_ranges_[write_index] = range;
            continue;
        }

        auto const range_end{range.first_index + range.count};
        merged_range.count = FMath::Max(merged_end, range_end) - merged_range.first_index;
    }

    dirty_ranges_.SetNum(write_index + 1, EAllowShrinking::No);
}

auto InstanceState::calculate_instance_bounds(int32 instance_index) const -> FBox3f {
    if (!has_mesh_bounds_) {
        return FBox3f{ForceInit};
    }

    auto const& scale{instance_data_.scales[instance_index]};
    auto const center{
        instance_data_.positions[instance_index] +
        instance_data_.rotations[instance_index].RotateVector(mesh_bounds_origin_ * scale)};
    auto const radius{mesh_bounds_radius_ * scale.GetAbsMax()};
    auto const extent{FVector3f{radius}};
    return FBox3f{center - extent, center + extent};
}

auto InstanceState::rebuild_bounds_tree() -> void {
    auto const instance_count{instance_data_.num()};
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

auto InstanceState::update_bounds_tree(TConstArrayView<FSandboxISMCDirtyRange> dirty_ranges)
    -> void {
    if (bounds_leaf_capacity_ == 0) {
        update_local_bounds_from_tree();
        return;
    }

    auto const updates_every_instance{dirty_ranges.Num() == 1 && dirty_ranges[0].first_index == 0 &&
                                      dirty_ranges[0].count == instance_data_.num()};
    if (updates_every_instance) {
        FBox3f local_box{ForceInit};
        auto const instance_count{instance_data_.num()};
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

auto InstanceState::update_local_bounds_from_tree() -> void {
    auto const has_valid_root{bounds_tree_.Num() > 1 && bounds_tree_[1].IsValid != 0};
    local_bounds_ = has_valid_root
                      ? FBoxSphereBounds{FBoxSphereBounds3f{bounds_tree_[1]}}
                      : FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0};
}
}
