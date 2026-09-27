#include "SandboxISMCInstanceChunkWriter.h"

auto FSandboxISMCInstanceChunkWriter::validate_transforms(TConstArrayView<FVector3f> positions,
                                                          TConstArrayView<FQuat4f> rotations,
                                                          TConstArrayView<FVector3f> scales) const
    -> bool {
    auto const count{instances_.Num()};
    if (positions.Num() != count || rotations.Num() != count || scales.Num() != count) {
        UE_LOG(LogTemp,
               Fatal,
               TEXT("SandboxISMC chunk at %d requires %d transforms; received %d positions, "
                    "%d rotations and %d scales"),
               first_index_,
               count,
               positions.Num(),
               rotations.Num(),
               scales.Num());
        return false;
    }
    if (count == 0) {
        return true;
    }

    // Valid endpoints plus domain membership guarantee safe unchecked integer conversion.
    for (int32 axis{0}; axis < 3; ++axis) {
        if (!position_bounds_.IsValid ||
            !(position_bounds_.Min[axis] <= position_bounds_.Max[axis]) ||
            !ml::sandbox_ismc::can_quantize_position(position_bounds_.Min[axis],
                                                     position_root_[axis]) ||
            !ml::sandbox_ismc::can_quantize_position(position_bounds_.Max[axis],
                                                     position_root_[axis])) {
            UE_LOG(LogTemp,
                   Fatal,
                   TEXT("SandboxISMC chunk at %d has invalid position domain on axis %d: "
                        "[%g, %g], root=%g (16 UU, +/-32767 offsets)"),
                   first_index_,
                   axis,
                   position_bounds_.Min[axis],
                   position_bounds_.Max[axis],
                   position_root_[axis]);
            return false;
        }
    }

    auto const* position_data{positions.GetData()};
    auto const* rotation_data{rotations.GetData()};
    auto const* scale_data{scales.GetData()};
    for (int32 local_index{0}; local_index < count; ++local_index) {
        auto const position{position_data[local_index]};
        auto const rotation{rotation_data[local_index]};
        auto const scale{scale_data[local_index]};
        // Ordered comparisons against the finite domain also reject NaN and infinity.
        if (!position_bounds_.IsInsideOrOn(position)) {
            UE_LOG(LogTemp,
                   Fatal,
                   TEXT("SandboxISMC instance %d requires a finite position inside the snapshot "
                        "domain; received (%g, %g, %g)"),
                   first_index_ + local_index,
                   position.X,
                   position.Y,
                   position.Z);
            return false;
        }
        if (!ml::sandbox_ismc::is_normalized_quaternion(
                ml::make_quaternion4f(rotation.X, rotation.Y, rotation.Z, rotation.W))) {
            UE_LOG(LogTemp,
                   Fatal,
                   TEXT("SandboxISMC instance %d requires finite normalized rotation (length "
                        "squared tolerance 1e-4); received (%g, %g, %g, %g)"),
                   first_index_ + local_index,
                   rotation.X,
                   rotation.Y,
                   rotation.Z,
                   rotation.W);
            return false;
        }
        // Ordered range comparisons also reject NaN and infinity.
        if (!supports_scale(scale)) {
            UE_LOG(LogTemp,
                   Fatal,
                   TEXT("SandboxISMC instance %d requires finite scale in [0, 31.875]; "
                        "received (%g, %g, %g)"),
                   first_index_ + local_index,
                   scale.X,
                   scale.Y,
                   scale.Z);
            return false;
        }
    }
    return true;
}

template <ESandboxISMCBoundsMode BoundsMode>
auto FSandboxISMCInstanceChunkWriter::set_transforms(TConstArrayView<FVector3f> positions,
                                                     TConstArrayView<FQuat4f> rotations,
                                                     TConstArrayView<FVector3f> scales) -> void {
    if (!validate_transforms(positions, rotations, scales)) {
        return;
    }

    auto const count{instances_.Num()};
    auto* const RESTRICT instances{instances_.GetData()};
    auto const* RESTRICT position_data{positions.GetData()};
    auto const* RESTRICT rotation_data{rotations.GetData()};
    auto const* RESTRICT scale_data{scales.GetData()};
    for (int32 local_index{0}; local_index < count; ++local_index) {
        auto const position{position_data[local_index]};
        auto const rotation{rotation_data[local_index]};
        auto const scale{scale_data[local_index]};
        auto& instance{instances[local_index]};
        auto const quaternion{
            ml::make_quaternion4f(rotation.X, rotation.Y, rotation.Z, rotation.W)};
        for (int32 axis{0}; axis < 3; ++axis) {
            instance.position[axis] =
                ml::sandbox_ismc::quantize_position_unchecked(position[axis], position_root_[axis]);
            instance.scale[axis] = ml::sandbox_ismc::pack_scale_unchecked(scale[axis]);
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
