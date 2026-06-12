#pragma once

#include <Containers/Array.h>
#include <Math/MathFwd.h>

struct FVectors3f;
struct FRotatorsf;

template <typename T>
struct TRotatorsView;

template <typename T>
struct TVectors3View;

namespace ml {
auto SANDBOXCORE_API make_transform(FVectors3f const& locations,
                                    FRotatorsf const& rotations,
                                    int32 const i) -> FTransform;
auto SANDBOXCORE_API make_transforms(FVectors3f const& locations, FRotatorsf const& rotations)
    -> TArray<FTransform>;
auto SANDBOXCORE_API make_transforms(TVectors3View<float const> const locations,
                                     TRotatorsView<float const> const rotations)
    -> TArray<FTransform>;
}
