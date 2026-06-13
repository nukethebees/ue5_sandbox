#include <SandboxCore/vector_math.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.Math.dot_product.Zero") {
    auto const result{ml::dot_product(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f)};

    REQUIRE(result == 0.0f);
}

TEST_CASE("SandboxCore.Math.dot_product.One") {
    auto const result{ml::dot_product(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f)};

    REQUIRE(result == 3.0f);
}

TEST_CASE("SandboxCore.Math.dot_product.MixedSigns") {
    auto const result{ml::dot_product(1, -1, 2, 2, 1, -1)};

    REQUIRE(result == -1);
}

TEST_CASE("SandboxCore.Math.dot_product.PointerZero") {
    TArray<float> const a_xs{0.0f, 0.0f};
    TArray<float> const a_ys{0.0f, 0.0f};
    TArray<float> const a_zs{0.0f, 0.0f};
    TArray<float> const b_xs{1.0f, 1.0f};
    TArray<float> const b_ys{1.0f, 2.0f};
    TArray<float> const b_zs{1.0f, 3.0f};
    TArray<float> out;
    out.SetNumUninitialized(2);

    ml::kernel::dot_product(
        out.GetData(), a_xs.GetData(), a_ys.GetData(), a_zs.GetData(), b_xs.GetData(), b_ys.GetData(), b_zs.GetData(), out.Num());

    TArray<float> const expected{0.0f, 0.0f};
    CHECK(out == expected);
}

TEST_CASE("SandboxCore.Math.dot_product.PointerMixedSigns") {
    TArray<float> const a_xs{1.0f, -1.0f, 2.0f};
    TArray<float> const a_ys{2.0f, 2.0f, -1.0f};
    TArray<float> const a_zs{3.0f, -2.0f, 1.0f};
    TArray<float> const b_xs{1.0f, 2.0f, -1.0f};
    TArray<float> const b_ys{1.0f, -1.0f, 2.0f};
    TArray<float> const b_zs{1.0f, 1.0f, 3.0f};
    TArray<float> out;
    out.SetNumUninitialized(3);

    ml::kernel::dot_product(
        out.GetData(), a_xs.GetData(), a_ys.GetData(), a_zs.GetData(), b_xs.GetData(), b_ys.GetData(), b_zs.GetData(), out.Num());

    TArray<float> const expected{6.0f, -6.0f, -1.0f};
    CHECK(out == expected);
}

TEST_CASE("SandboxCore.Math.dot_product.PointerEmpty") {
    TArray<float> const a_xs{};
    TArray<float> const a_ys{};
    TArray<float> const a_zs{};
    TArray<float> const b_xs{};
    TArray<float> const b_ys{};
    TArray<float> const b_zs{};
    TArray<float> out{};

    ml::kernel::dot_product(
        out.GetData(), a_xs.GetData(), a_ys.GetData(), a_zs.GetData(), b_xs.GetData(), b_ys.GetData(), b_zs.GetData(), out.Num());

    CHECK(out.IsEmpty());
}

TEST_CASE("SandboxCore.Math.dot_product.ArrayView") {
    TArray<float> const a_xs{1.0f, -1.0f};
    TArray<float> const a_ys{2.0f, 2.0f};
    TArray<float> const a_zs{3.0f, -2.0f};
    TArray<float> const b_xs{1.0f, 2.0f};
    TArray<float> const b_ys{1.0f, -1.0f};
    TArray<float> const b_zs{1.0f, 1.0f};
    TArray<float> out;
    out.SetNumUninitialized(2);

    ml::dot_product(TArrayView<float>{out},
                    TConstArrayView<float>{a_xs},
                    TConstArrayView<float>{a_ys},
                    TConstArrayView<float>{a_zs},
                    TConstArrayView<float>{b_xs},
                    TConstArrayView<float>{b_ys},
                    TConstArrayView<float>{b_zs});

    TArray<float> const expected{6.0f, -6.0f};
    CHECK(out == expected);
}
