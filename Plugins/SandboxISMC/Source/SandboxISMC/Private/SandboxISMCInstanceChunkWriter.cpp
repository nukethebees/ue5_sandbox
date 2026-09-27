#include "SandboxISMCInstanceChunkWriter.h"

template <ESandboxISMCBoundsMode BoundsMode>
auto FSandboxISMCInstanceChunkWriter::set_transforms(TConstArrayView<FVector3f> positions,
                                                     TConstArrayView<FQuat4f> rotations,
                                                     TConstArrayView<FVector3f> scales) -> void {
    auto const count{instances_.Num()};
    check(positions.Num() == count && rotations.Num() == count && scales.Num() == count);
    auto* const RESTRICT instances{instances_.GetData()};
    auto const* RESTRICT position_data{positions.GetData()};
    auto const* RESTRICT rotation_data{rotations.GetData()};
    auto const* RESTRICT scale_data{scales.GetData()};
    for (int32 local_index{0}; local_index < count; ++local_index) {
        auto const position{position_data[local_index]};
        auto const rotation{rotation_data[local_index]};
        auto const scale{scale_data[local_index]};
        auto& instance{instances[local_index]};
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

        if constexpr (BoundsMode == ESandboxISMCBoundsMode::Calculate) {
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
}

template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Calculate>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>, TConstArrayView<FVector3f>) -> void;
template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Supplied>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>, TConstArrayView<FVector3f>) -> void;
