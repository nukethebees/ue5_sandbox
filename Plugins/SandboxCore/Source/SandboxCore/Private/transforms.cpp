#include <SandboxCore/transforms.h>

#include <SandboxCore/array_utils.h>
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
auto make_transforms(TVectors3View<float const> const locations,
                     TRotatorsView<float const> const rotations) -> TArray<FTransform> {
    auto const n{ml::num(locations)};
    check(n == ml::num(rotations));

    TArray<FTransform> out;
    for (int32 i{0}; i < n; ++i) {
        out.Emplace(ml::get_rotator3d(rotations, i), ml::get_vector3d(locations, i));
    }

    return out;
}

}
