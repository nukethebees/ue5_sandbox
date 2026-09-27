#pragma once

#include "SandboxISMCInstanceRange.h"
#include "SandboxISMCRenderInstance.h"

#include "Containers/ArrayView.h"
#include "Math/Box.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

class SANDBOXISMC_API FSandboxISMCInstanceChunkWriter final {
  public:
    FSandboxISMCInstanceChunkWriter(TArrayView<FSandboxISMCRenderInstance> instances,
                                    TArrayView<float> custom_data,
                                    int32 num_custom_data_floats,
                                    int32 first_index,
                                    FBox3f position_bounds,
                                    FVector3f position_root,
                                    FVector3f mesh_bounds_origin,
                                    FVector3f mesh_bounds_extent,
                                    bool has_mesh_bounds)
        : instances_{instances}
        , custom_data_{custom_data}
        , num_custom_data_floats_{num_custom_data_floats}
        , first_index_{first_index}
        , position_bounds_{position_bounds}
        , position_root_{position_root}
        , mesh_bounds_origin_{mesh_bounds_origin}
        , mesh_bounds_extent_{mesh_bounds_extent}
        , has_mesh_bounds_{has_mesh_bounds} {
        check(num_custom_data_floats >= 0);
        check(custom_data.Num() == instances.Num() * num_custom_data_floats);
    }

    auto first_index() const -> int32 { return first_index_; }
    auto num() const -> int32 { return instances_.Num(); }
    auto range() const -> FSandboxISMCInstanceRange { return {first_index_, instances_.Num()}; }
    auto num_custom_data_floats() const -> int32 { return num_custom_data_floats_; }

    static auto supports_scale(FVector3f scale) -> bool {
        return scale.X >= 0.0f && scale.Y >= 0.0f && scale.Z >= 0.0f && scale.X <= 31.875f &&
               scale.Y <= 31.875f && scale.Z <= 31.875f;
    }

    auto custom_data(int32 local_index) -> TArrayView<float> {
        check(instances_.IsValidIndex(local_index));
        return custom_data_.Slice(local_index * num_custom_data_floats_, num_custom_data_floats_);
    }

    auto set_transform(int32 local_index, FVector3f position, FQuat4f rotation, FVector3f scale)
        -> void {
        check(instances_.IsValidIndex(local_index));

        auto& instance{instances_[local_index]};
        checkfSlow(position_bounds_.IsInsideOrOn(position),
                   TEXT("SandboxISMC instance %d position is outside the snapshot domain"),
                   first_index_ + local_index);
        auto const quaternion{
            ml::make_quaternion4f(rotation.X, rotation.Y, rotation.Z, rotation.W)};
        checkfSlow(
            ml::sandbox_ismc::is_normalized_quaternion(quaternion),
            TEXT(
                "SandboxISMC requires finite normalized rotation (length squared tolerance 1e-4)"));
        for (int32 axis{0}; axis < 3; ++axis) {
            auto const offset{
                ml::sandbox_ismc::quantize_position(position[axis], position_root_[axis])};
            auto const packed_scale{ml::sandbox_ismc::pack_scale(scale[axis])};
            if (!offset || !packed_scale) [[unlikely]] {
                UE_LOG(LogTemp,
                       Fatal,
                       TEXT("SandboxISMC instance %d axis %d cannot pack position=%g root=%g (16 "
                            "UU, +/-524272), scale=%g (0..31.875)"),
                       first_index_ + local_index,
                       axis,
                       position[axis],
                       position_root_[axis],
                       scale[axis]);
            }
            instance.position[axis] = *offset;
            instance.scale[axis] = *packed_scale;
        }
        instance.rotation = ml::sandbox_ismc::pack_normalized_quat32(quaternion);
        instance.reserved_0 = 0;
        instance.reserved_1 = 0;

        if (has_mesh_bounds_) {
            auto const row_0{rotation.RotateVector(FVector3f::ForwardVector) * scale.X};
            auto const row_1{rotation.RotateVector(FVector3f::RightVector) * scale.Y};
            auto const row_2{rotation.RotateVector(FVector3f::UpVector) * scale.Z};
            auto const center{position + row_0 * mesh_bounds_origin_.X +
                              row_1 * mesh_bounds_origin_.Y + row_2 * mesh_bounds_origin_.Z};
            auto const extent{row_0.GetAbs() * mesh_bounds_extent_.X +
                              row_1.GetAbs() * mesh_bounds_extent_.Y +
                              row_2.GetAbs() * mesh_bounds_extent_.Z};
            bounds_ += FBox3f{center - extent, center + extent};
        }
    }

    // Source geometry bounds. The component adds codec error once per snapshot.
    auto bounds() const -> FBox3f const& { return bounds_; }

    static auto expand_render_bounds(FBox3f source, FVector3f mesh_origin, FVector3f mesh_extent)
        -> FBox3f {
        if (!source.IsValid) {
            return source;
        }
        auto const radius{(mesh_origin.GetAbs() + mesh_extent).Size()};
        auto const codec_error{ml::sandbox_ismc::geometry_error(radius)};
        auto const magnitude{source.Min.GetAbs().ComponentMax(source.Max.GetAbs())};
        auto const margin{FVector3f{codec_error} +
                          (magnitude + FVector3f{codec_error + 1.0f}) * (16.0f * FLT_EPSILON)};
        return FBox3f{source.Min - margin, source.Max + margin};
    }
  private:
    TArrayView<FSandboxISMCRenderInstance> instances_;
    TArrayView<float> custom_data_;
    FBox3f bounds_{ForceInit};
    int32 num_custom_data_floats_{0};
    int32 first_index_{0};
    FBox3f position_bounds_{ForceInit};
    FVector3f position_root_{FVector3f::ZeroVector};
    FVector3f mesh_bounds_origin_{FVector3f::ZeroVector};
    FVector3f mesh_bounds_extent_{FVector3f::ZeroVector};
    bool has_mesh_bounds_{false};
};
