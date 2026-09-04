#pragma once

#include <Math/Box.h>
#include <Math/Transform.h>

class UStaticMesh;
struct FKAggregateGeom;

namespace ml {
auto SGCOLLISION_API get_aabb(FKAggregateGeom const& geometry, FTransform const& local_to_world)
    -> FBox;
auto SGCOLLISION_API get_aabb(UStaticMesh const& mesh) -> FBox;
}
