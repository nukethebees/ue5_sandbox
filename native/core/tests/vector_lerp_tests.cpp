#include <ioj/generated/vector_lerp_kernels.h>

#include <gtest/gtest.h>

#include <array>

using NativeCoreVectorLerp = testing::TestWithParam<int>;

TEST_P(NativeCoreVectorLerp, ScalarAndArrayAlpha) {
    auto const dimensions{GetParam()};
    std::array<std::array<float, 3>, 3> from{{{0, 10, -4}, {5, 10, 4}, {-2, 8, 0}}};
    std::array<std::array<float, 3>, 3> const to{{{10, 20, 4}, {15, 0, -4}, {2, 0, 12}}};
    std::array<std::array<float, 3>, 3> const expected{{{0, 15, 4}, {5, 5, -4}, {-2, 4, 12}}};
    std::array<float, 3> const alpha{0, .5f, 1};
    if (dimensions == 2) {
        ioj::lerp_2d_in_place(from[0], from[1], to[0], to[1], alpha);
    } else {
        ioj::lerp_3d_in_place(from[0], from[1], from[2], to[0], to[1], to[2], alpha);
    }

    std::array<std::array<double, 2>, 3> const from_double{{{0, 10}, {2, 6}, {8, -4}}};
    std::array<std::array<double, 2>, 3> const to_double{{{4, 2}, {6, 10}, {0, 8}}};
    std::array<std::array<double, 2>, 3> const expected_double{{{1, 8}, {3, 7}, {6, -1}}};
    std::array<std::array<double, 2>, 3> output{};
    if (dimensions == 2) {
        ioj::lerp_2d(
            from_double[0], from_double[1], to_double[0], to_double[1], .25, output[0], output[1]);
    } else {
        ioj::lerp_3d(from_double[0],
                    from_double[1],
                    from_double[2],
                    to_double[0],
                    to_double[1],
                    to_double[2],
                    .25,
                    output[0],
                    output[1],
                    output[2]);
    }
    for (auto axis{0}; axis < dimensions; ++axis) {
        EXPECT_EQ(from[axis], expected[axis]) << axis;
        EXPECT_EQ(output[axis], expected_double[axis]) << axis;
    }
}

INSTANTIATE_TEST_SUITE_P(Dimensions,
                         NativeCoreVectorLerp,
                         testing::Values(2, 3),
                         [](auto const& info) {
                             return info.param == 2 ? "TwoDimensions" : "ThreeDimensions";
                         });

TEST(NativeCoreVectorLerpEmpty, SupportsThreeDimensionalRanges) {
    std::array<std::array<float, 0>, 3> current{};
    std::array<std::array<float, 0>, 3> const target{};
    ioj::lerp_3d_in_place(current[0], current[1], current[2], target[0], target[1], target[2], .5f);
}
