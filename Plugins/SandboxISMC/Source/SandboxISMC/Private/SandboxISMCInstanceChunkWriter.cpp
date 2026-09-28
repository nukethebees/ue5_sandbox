#include "SandboxISMCInstanceChunkWriter.h"

#include "sandbox/core/sandbox_ismc_packing.h"

static_assert(sizeof(FVector3f) == 3 * sizeof(float));
static_assert(offsetof(FVector3f, X) == 0 && offsetof(FVector3f, Y) == sizeof(float) &&
              offsetof(FVector3f, Z) == 2 * sizeof(float));
static_assert(sizeof(FQuat4f) == 4 * sizeof(float));
static_assert(offsetof(FQuat4f, X) == 0 && offsetof(FQuat4f, Y) == sizeof(float) &&
              offsetof(FQuat4f, Z) == 2 * sizeof(float) &&
              offsetof(FQuat4f, W) == 3 * sizeof(float));

auto FSandboxISMCInstanceChunkWriter::validate_transforms(TConstArrayView<FVector3f> positions,
                                                          TConstArrayView<FQuat4f> rotations) const
    -> bool {
    auto const count{instances_.Num()};
    if (positions.Num() != count || rotations.Num() != count) {
        UE_LOG(LogTemp,
               Fatal,
               TEXT("SandboxISMC chunk at %d requires %d transforms; received %d positions, "
                    "%d rotations"),
               first_index_,
               count,
               positions.Num(),
               rotations.Num());
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
    for (int32 local_index{0}; local_index < count; ++local_index) {
        auto const position{position_data[local_index]};
        auto const rotation{rotation_data[local_index]};
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
    }
    return true;
}

template <ESandboxISMCBoundsMode BoundsMode>
auto FSandboxISMCInstanceChunkWriter::set_transforms(TConstArrayView<FVector3f> positions,
                                                     TConstArrayView<FQuat4f> rotations) -> void {
    check(validate_transforms(positions, rotations));

    auto const count{static_cast<std::size_t>(instances_.Num())};
    auto const native_vector{
        [](FVector3f value) { return ml::make_vector3f(value.X, value.Y, value.Z); }};
    ml::sandbox_ismc::TransformInput const input{
        std::as_bytes(std::span{positions.GetData(), count}),
        std::as_bytes(std::span{rotations.GetData(), count})};
    ml::sandbox_ismc::PackingParameters const parameters{native_vector(position_root_),
                                                         native_vector(mesh_bounds_origin_),
                                                         native_vector(mesh_bounds_extent_)};
    auto const output{std::span{instances_.GetData(), count}};
    if constexpr (BoundsMode == ESandboxISMCBoundsMode::Calculate) {
        ml::sandbox_ismc::TransformBounds bounds{};
        ml::sandbox_ismc::pack_transforms(input, parameters, output, &bounds);
        bounds_ = bounds.valid
                    ? FBox3f{FVector3f{bounds.minimum.X, bounds.minimum.Y, bounds.minimum.Z},
                             FVector3f{bounds.maximum.X, bounds.maximum.Y, bounds.maximum.Z}}
                    : FBox3f{ForceInit};
    } else {
        ml::sandbox_ismc::pack_transforms(input, parameters, output);
    }
}

template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Calculate>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>) -> void;
template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Supplied>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>) -> void;
