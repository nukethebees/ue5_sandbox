#pragma once

#include <ioj/sim/entity_aabbs.h>

#include <CoreMinimal.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

namespace ml::ioj {
struct FEntityAABBs : ::ioj::sim::collision::EntityAABBs {
    void set_centre(int32 const index, FVector3f const centre) noexcept {
        EntityAABBs::set_centre(index, ml::to_native(centre));
    }

    void set_half_extents(int32 const index, FVector3f const half_extents) noexcept {
        EntityAABBs::set_half_extents(index, ml::to_native(half_extents));
    }

    auto get_centre(int32 const index) const noexcept -> FVector3f {
        auto const value{EntityAABBs::get_centre(index)};
        return {value.X, value.Y, value.Z};
    }

    auto get_half_extents(int32 const index) const noexcept -> FVector3f {
        auto const value{EntityAABBs::get_half_extents(index)};
        return {value.X, value.Y, value.Z};
    }
};
} // namespace ml::ioj
