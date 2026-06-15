#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Transforms.set_transform_locations.PreservesRotationAndScale") {
    TArray<FQuat> rotations{
        FRotator{10.0, 20.0, 30.0}.Quaternion(),
        FRotator{40.0, 50.0, 60.0}.Quaternion(),
    };

    TArray<FVector> locations{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };

    TArray<FVector> scales{
        {2.0, 3.0, 4.0},
        {5.0, 6.0, 7.0},
    };

    auto const n{scales.Num()};

    TArray<FTransform> transforms;
    for (int32 i{0}; i < n; ++i) {
        transforms.Emplace(rotations[i], locations[i], scales[i]);
    }

    auto const new_locations{ml::make_vectors3f({7.0f, 10.0f}, {8.0f, 11.0f}, {9.0f, 12.0f})};

    ml::set_transform_locations(TArrayView<FTransform>{transforms}, new_locations);

    for (int32 i{0}; i < n; ++i) {
        CHECK(transforms[i].GetRotation() == rotations[i]);
        CHECK(transforms[i].GetLocation() == ml::get_vector3d(new_locations, i));
        CHECK(transforms[i].GetScale3D() == scales[i]);
    }
}

TEST_CASE("SandboxCore.Transforms.set_transform_locations.Empty") {
    TArray<FTransform> transforms;
    FVectors3f const locations;

    ml::set_transform_locations(TArrayView<FTransform>{transforms}, locations);

    CHECK(transforms.IsEmpty());
}
