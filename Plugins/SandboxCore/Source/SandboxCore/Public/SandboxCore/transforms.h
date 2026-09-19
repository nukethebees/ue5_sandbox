#pragma once

#include <ioj/sim/rotators3f.h>
#include <ioj/sim/vectors3f.h>

#include <Containers/Array.h>
#include <Math/MathFwd.h>

namespace ml {
auto SANDBOXCORE_API make_transforms(::ioj::sim::Vectors3fConstView const locations,
                                     ::ioj::sim::Rotators3fConstView const rotations)
    -> TArray<FTransform>;

void SANDBOXCORE_API set_transform_locations(TArrayView<FTransform> const transforms,
                                             ::ioj::sim::Vectors3fConstView locations);
}
