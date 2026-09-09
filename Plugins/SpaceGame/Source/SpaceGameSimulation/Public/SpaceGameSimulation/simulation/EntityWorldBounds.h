#pragma once

#include <SpaceGameSimulation/simulation/EntityAABBs.h>

namespace ml::ioj {
// Enclose the rotated local collision box, including its offset from the entity pivot.
inline auto make_entity_world_bounds(FEntityAABBs const& bounds,
                                     int32 const type_index,
                                     FVector3f const position,
                                     FRotator3f const rotation) -> FBox3f {
    auto const orientation{rotation.Quaternion()};
    auto const centre{position + orientation.RotateVector(bounds.get_centre(type_index))};
    auto const local_extent{bounds.get_half_extents(type_index)};
    auto const extent{orientation.RotateVector(FVector3f{local_extent.X, 0.f, 0.f}).GetAbs() +
                      orientation.RotateVector(FVector3f{0.f, local_extent.Y, 0.f}).GetAbs() +
                      orientation.RotateVector(FVector3f{0.f, 0.f, local_extent.Z}).GetAbs()};
    return FBox3f{centre - extent, centre + extent};
}
}
