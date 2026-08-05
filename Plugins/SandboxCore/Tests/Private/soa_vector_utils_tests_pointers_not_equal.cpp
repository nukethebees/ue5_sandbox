#include <SandboxCore/soa_vector_utils.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.Two vectors") {
    auto const first{
        ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};
    auto const second{
        ml::make_vectors3f(TArray<FVector3f>{{4.0f, 5.0f, 6.0f}})};

    CHECK(ml::pointers_not_equal(first, second));
}

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.Variadic mixed views") {
    auto first{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};
    auto second{ml::make_vectors3f(TArray<FVector3f>{{4.0f, 5.0f, 6.0f}})};
    auto third{ml::make_vectors3f(TArray<FVector3f>{{7.0f, 8.0f, 9.0f}})};
    auto fourth{ml::make_vectors3f(TArray<FVector3f>{{10.0f, 11.0f, 12.0f}})};

    auto first_view{first.get_view()};
    auto const second_view{second.get_const_view()};
    auto third_view{third.get_view()};

    CHECK(ml::pointers_not_equal(first_view, second_view, third_view, fourth));
}

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.Same vector") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};

    CHECK_FALSE(ml::pointers_not_equal(vectors, vectors));
}

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.Aliased view") {
    auto vectors{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};
    auto view{vectors.get_view()};

    CHECK_FALSE(ml::pointers_not_equal(vectors, view));
}

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.One aliased variadic pair") {
    auto first{ml::make_vectors3f(TArray<FVector3f>{{1.0f, 2.0f, 3.0f}})};
    auto second{ml::make_vectors3f(TArray<FVector3f>{{4.0f, 5.0f, 6.0f}})};
    auto third{ml::make_vectors3f(TArray<FVector3f>{{7.0f, 8.0f, 9.0f}})};
    auto third_view{third.get_const_view()};

    CHECK_FALSE(ml::pointers_not_equal(first, second, third, third_view));
}

TEST_CASE("SandboxCore.SoaVectorUtils.pointers_not_equal.Empty vectors share null storage") {
    FVectors3f first;
    FVectors3f second;

    CHECK_FALSE(ml::pointers_not_equal(first, second));
}
