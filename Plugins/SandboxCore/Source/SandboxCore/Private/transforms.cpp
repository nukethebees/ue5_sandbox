#include <SandboxCore/transforms.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/soa_vectors.h>

#include <CoreMinimal.h>

struct FVectors3f;
struct FRotatorsf;

namespace ml {
auto make_transform(FVectors3f const& locations, FRotatorsf const& rotations, int32 const i)
    -> FTransform {
    return {ml::get_rotator3d(rotations, i), ml::get_vector3d(locations, i)};
}
auto make_transforms(FVectors3f const& locations, FRotatorsf const& rotations)
    -> TArray<FTransform> {
    return make_transforms(locations.get_const_view(), rotations.get_const_view());
}
auto make_transforms(FVectors3f::ConstView const locations,
                     TRotatorsView<float const> const rotations) -> TArray<FTransform> {
    auto const n{ml::num(locations)};
    check(ml::all_num_equal(locations, rotations));

    TArray<FTransform> out;
    for (int32 i{0}; i < n; ++i) {
        out.Emplace(ml::get_rotator3d(rotations, i), ml::get_vector3d(locations, i));
    }

    return out;
}
auto make_transforms(::ioj::sim::Vectors3fConstView const locations,
                     ::ioj::sim::Rotators3fConstView const rotations) -> TArray<FTransform> {
    auto const n{locations.num()};
    check(n == rotations.num());

    TArray<FTransform> out;
    out.Reserve(n);
    for (int32 i{}; i < n; ++i) {
        out.Emplace(FRotator3d{rotations.pitches[i], rotations.yaws[i], rotations.rolls[i]},
                    FVector3d{locations.xs[i], locations.ys[i], locations.zs[i]});
    }

    return out;
}

void set_transform_locations(TArrayView<FTransform> const transforms, FVectors3f const& locations) {
    auto const n{ml::num(transforms)};

    check(ml::all_num_equal(transforms, locations));

    for (int32 i{0}; i < n; ++i) {
        transforms[i].SetLocation(ml::get_vector3d(locations, i));
    }
}

}
