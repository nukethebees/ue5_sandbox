#pragma once

#include <ioj/sim/entity_world_bounds.h>

#include <SpaceGameSimulation/simulation/EntityAABBs.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

namespace ml::ioj {
inline auto make_entity_world_bounds(FEntityAABBs const& bounds,
                                     int32 const type_index,
                                     FVector3f const position,
                                     FRotator3f const rotation) noexcept
    -> ::ioj::sim::collision::WorldAABB {
    auto const orientation{rotation.Quaternion()};
    return ::ioj::sim::collision::make_entity_world_bounds(
        bounds,
        type_index,
        ml::to_native(position),
        make_quaternion4f(orientation.X, orientation.Y, orientation.Z, orientation.W));
}

inline auto to_unreal(::ioj::sim::collision::WorldAABB const bounds) noexcept -> FBox3f {
    return {ml::to_unreal(bounds.min), ml::to_unreal(bounds.max)};
}
}
