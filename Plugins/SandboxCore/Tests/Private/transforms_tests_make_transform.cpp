#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/transforms.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Transforms.make_transform.Index") {
    TArray<FQuat> expected_rotations{
        FRotator{10.0, 20.0, 30.0}.Quaternion(),
        FRotator{40.0, 50.0, 60.0}.Quaternion(),
    };

    TArray<FVector> expected_locations{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };

    auto const locations{ml::make_vectors3f({1.0f, 4.0f}, {2.0f, 5.0f}, {3.0f, 6.0f})};
    auto const rotations{ml::make_rotatorsf({10.0f, 40.0f}, {20.0f, 50.0f}, {30.0f, 60.0f})};
    int32 const i{1};

    auto const result{ml::make_transform(locations, rotations, i)};

    CHECK(result.GetRotation() == expected_rotations[i]);
    CHECK(result.GetLocation() == expected_locations[i]);
    CHECK(result.GetScale3D() == FVector::OneVector);
}

TEST_CASE("SandboxCore.Transforms.make_transforms.Arrays") {
    TArray<FQuat> expected_rotations{
        FRotator{10.0, 20.0, 30.0}.Quaternion(),
        FRotator{40.0, 50.0, 60.0}.Quaternion(),
    };

    TArray<FVector> expected_locations{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
    };

    auto const n{expected_locations.Num()};

    auto const locations{ml::make_vectors3f({1.0f, 4.0f}, {2.0f, 5.0f}, {3.0f, 6.0f})};
    auto const rotations{ml::make_rotatorsf({10.0f, 40.0f}, {20.0f, 50.0f}, {30.0f, 60.0f})};

    auto const result{ml::make_transforms(locations, rotations)};

    CHECK(result.Num() == n);

    for (int32 i{0}; i < n; ++i) {
        CHECK(result[i].GetRotation() == expected_rotations[i]);
        CHECK(result[i].GetLocation() == expected_locations[i]);
        CHECK(result[i].GetScale3D() == FVector::OneVector);
    }
}

TEST_CASE("SandboxCore.Transforms.make_transforms.View") {
    TArray<FQuat> expected_rotations{
        FRotator{40.0, 50.0, 60.0}.Quaternion(),
        FRotator{70.0, 80.0, 90.0}.Quaternion(),
    };

    TArray<FVector> expected_locations{
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0},
    };

    auto const n{expected_locations.Num()};

    auto const locations{
        ml::make_vectors3f({1.0f, 4.0f, 7.0f}, {2.0f, 5.0f, 8.0f}, {3.0f, 6.0f, 9.0f})};
    auto const rotations{ml::make_rotatorsf({10.0f, 40.0f, 70.0f},
                                            {20.0f, 50.0f, 80.0f},
                                            {30.0f, 60.0f, 90.0f})};

    auto const result{ml::make_transforms(locations.get_const_view().right(2),
                                          rotations.get_const_view().right(2))};

    CHECK(result.Num() == n);

    for (int32 i{0}; i < n; ++i) {
        CHECK(result[i].GetRotation() == expected_rotations[i]);
        CHECK(result[i].GetLocation() == expected_locations[i]);
        CHECK(result[i].GetScale3D() == FVector::OneVector);
    }
}

TEST_CASE("SandboxCore.Transforms.make_transforms.Empty") {
    FVectors3f const locations;
    FRotatorsf const rotations;

    auto const result{ml::make_transforms(locations, rotations)};

    CHECK(result.IsEmpty());
}
