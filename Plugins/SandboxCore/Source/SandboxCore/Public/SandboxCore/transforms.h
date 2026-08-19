#pragma once

#include <SandboxCore/soa_vectors_3f.h>

#include <Containers/Array.h>
#include <Math/MathFwd.h>

struct FRotatorsf;

template <typename T>
struct TRotatorsView;

namespace ml {
auto SANDBOXCORE_API make_transform(FVectors3f const& locations,
                                    FRotatorsf const& rotations,
                                    int32 const i) -> FTransform;
auto SANDBOXCORE_API make_transforms(FVectors3f const& locations, FRotatorsf const& rotations)
    -> TArray<FTransform>;
auto SANDBOXCORE_API make_transforms(FVectors3f::ConstView const locations,
                                     TRotatorsView<float const> const rotations)
    -> TArray<FTransform>;

void SANDBOXCORE_API set_transform_locations(TArrayView<FTransform> const transforms,
                                             FVectors3f const& locations);
}
