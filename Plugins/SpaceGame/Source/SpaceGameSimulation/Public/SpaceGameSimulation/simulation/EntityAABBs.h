#pragma once

#include <sandbox/simulation/entity_aabbs.h>

#include <CoreMinimal.h>

namespace ml::ioj {
struct FEntityAABBs : simulation::collision::EntityAABBs {
    auto get_centre(int32 const index) const noexcept -> FVector3f {
        auto const value{EntityAABBs::get_centre(index)};
        return {value.x, value.y, value.z};
    }

    auto get_half_extents(int32 const index) const noexcept -> FVector3f {
        auto const value{EntityAABBs::get_half_extents(index)};
        return {value.x, value.y, value.z};
    }
};
}
