#include "SandboxISMCInstanceChunkWriter.h"

#include "ioj/ismc/packing.h"

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

    auto const* position_data{positions.GetData()};
    auto const* rotation_data{rotations.GetData()};
    for (int32 local_index{0}; local_index < count; ++local_index) {
        auto const position{position_data[local_index]};
        auto const rotation{rotation_data[local_index]};
        if (position.ContainsNaN()) {
            UE_LOG(
                LogTemp,
                Fatal,
                TEXT("SandboxISMC instance %d requires a finite position; received (%g, %g, %g)"),
                first_index_ + local_index,
                position.X,
                position.Y,
                position.Z);
            return false;
        }
        if (!ioj::ismc::is_normalized_quaternion(
                ioj::Quaternion4f{rotation.X, rotation.Y, rotation.Z, rotation.W})) {
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
        [](FVector3f value) { return ioj::Vector3f{value.X, value.Y, value.Z}; }};
    ioj::ismc::TransformInput const input{std::as_bytes(std::span{positions.GetData(), count}),
                                          std::as_bytes(std::span{rotations.GetData(), count})};
    ioj::ismc::PackingParameters const parameters{native_vector(mesh_bounds_origin_),
                                                  native_vector(mesh_bounds_extent_)};
    auto const output{std::span{instances_.GetData(), count}};
    if constexpr (BoundsMode == ESandboxISMCBoundsMode::Calculate) {
        ioj::ismc::TransformBounds bounds{};
        bounds = ioj::ismc::TransformPacker{parameters, ioj::ismc::BoundsMode::Calculate}
                     .pack_transforms(input, output);
        bounds_ = !bounds.empty()
                    ? FBox3f{FVector3f{bounds.minimum.x(), bounds.minimum.y(), bounds.minimum.z()},
                             FVector3f{bounds.maximum.x(), bounds.maximum.y(), bounds.maximum.z()}}
                    : FBox3f{ForceInit};
    } else {
        ioj::ismc::TransformPacker{parameters, ioj::ismc::BoundsMode::Skip}.pack_transforms(input,
                                                                                            output);
    }
}

template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Calculate>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>) -> void;
template SANDBOXISMC_API auto
    FSandboxISMCInstanceChunkWriter::set_transforms<ESandboxISMCBoundsMode::Supplied>(
        TConstArrayView<FVector3f>, TConstArrayView<FQuat4f>) -> void;
