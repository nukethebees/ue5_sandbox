#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.lerp_in_place.AlphaValues") {
    auto current{ml::make_vectors3f({0.0f, 10.0f, 20.0f},
                                    {30.0f, 40.0f, 50.0f},
                                    {60.0f, 70.0f, 80.0f})};
    auto const target{ml::make_vectors3f({100.0f, 110.0f, 120.0f},
                                         {130.0f, 140.0f, 150.0f},
                                         {160.0f, 170.0f, 180.0f})};
    TArray<float> alpha{0.0f, 0.5f, 1.0f};

    ml::lerp_in_place(current, target, TArrayView<float>{alpha});

    auto const expected{ml::make_vectors3f({0.0f, 60.0f, 120.0f},
                                           {30.0f, 90.0f, 150.0f},
                                           {60.0f, 120.0f, 180.0f})};

    CHECK(ml::almost_equal(current, expected));
}

TEST_CASE("SandboxCore.SoaVectorUtils.lerp_in_place.AlphaScalar") {
    auto current{ml::make_vectors3f({0.0f, 10.0f},
                                    {20.0f, 30.0f},
                                    {40.0f, 50.0f})};
    auto const target{ml::make_vectors3f({100.0f, 110.0f},
                                         {120.0f, 130.0f},
                                         {140.0f, 150.0f})};

    ml::lerp_in_place(current, target, 0.25f);

    auto const expected{ml::make_vectors3f({25.0f, 35.0f},
                                           {45.0f, 55.0f},
                                           {65.0f, 75.0f})};

    CHECK(ml::almost_equal(current, expected));
}
