#include <sandbox/core/generated/vector_lerp_kernels.h>

#include <gtest/gtest.h>

#include <array>
#include <span>

TEST(NativeCoreVectorLerp, SupportsTwoDimensionalScalarAndArrayAlpha) {
    std::array<float, 3> from_x{0.f, 10.f, -4.f};
    std::array<float, 3> from_y{5.f, 10.f, 4.f};
    std::array<float, 3> const to_x{10.f, 20.f, 4.f};
    std::array<float, 3> const to_y{15.f, 0.f, -4.f};
    std::array<float, 3> const alpha{0.f, .5f, 1.f};

    ml::lerp_2d_in_place(from_x, from_y, to_x, to_y, alpha);

    EXPECT_EQ(from_x, (std::array<float, 3>{0.f, 15.f, 4.f}));
    EXPECT_EQ(from_y, (std::array<float, 3>{5.f, 5.f, -4.f}));

    std::array<double, 2> const from_double_x{0., 10.};
    std::array<double, 2> const from_double_y{2., 6.};
    std::array<double, 2> const to_double_x{4., 2.};
    std::array<double, 2> const to_double_y{6., 10.};
    std::array<double, 2> out_double_x{};
    std::array<double, 2> out_double_y{};

    ml::lerp_2d(
        from_double_x, from_double_y, to_double_x, to_double_y, .25, out_double_x, out_double_y);

    EXPECT_EQ(out_double_x, (std::array<double, 2>{1., 8.}));
    EXPECT_EQ(out_double_y, (std::array<double, 2>{3., 7.}));
}

TEST(NativeCoreVectorLerp, SupportsThreeDimensionalScalarAndArrayAlpha) {
    std::array<float, 3> from_x{0.f, 10.f, -4.f};
    std::array<float, 3> from_y{5.f, 10.f, 4.f};
    std::array<float, 3> from_z{-2.f, 8.f, 0.f};
    std::array<float, 3> const to_x{10.f, 20.f, 4.f};
    std::array<float, 3> const to_y{15.f, 0.f, -4.f};
    std::array<float, 3> const to_z{2.f, 0.f, 12.f};
    std::array<float, 3> const alpha{0.f, .5f, 1.f};

    ml::lerp_3d_in_place(from_x, from_y, from_z, to_x, to_y, to_z, alpha);

    EXPECT_EQ(from_x, (std::array<float, 3>{0.f, 15.f, 4.f}));
    EXPECT_EQ(from_y, (std::array<float, 3>{5.f, 5.f, -4.f}));
    EXPECT_EQ(from_z, (std::array<float, 3>{-2.f, 4.f, 12.f}));

    std::array<double, 2> const from_double_x{0., 10.};
    std::array<double, 2> const from_double_y{2., 6.};
    std::array<double, 2> const from_double_z{8., -4.};
    std::array<double, 2> const to_double_x{4., 2.};
    std::array<double, 2> const to_double_y{6., 10.};
    std::array<double, 2> const to_double_z{0., 8.};
    std::array<double, 2> out_double_x{};
    std::array<double, 2> out_double_y{};
    std::array<double, 2> out_double_z{};

    ml::lerp_3d(from_double_x,
                from_double_y,
                from_double_z,
                to_double_x,
                to_double_y,
                to_double_z,
                .25,
                out_double_x,
                out_double_y,
                out_double_z);

    EXPECT_EQ(out_double_x, (std::array<double, 2>{1., 8.}));
    EXPECT_EQ(out_double_y, (std::array<double, 2>{3., 7.}));
    EXPECT_EQ(out_double_z, (std::array<double, 2>{6., -1.}));
}

TEST(NativeCoreVectorLerp, SupportsEmptyThreeDimensionalRanges) {
    std::array<float, 0> current_x{};
    std::array<float, 0> current_y{};
    std::array<float, 0> current_z{};
    std::array<float, 0> const target_x{};
    std::array<float, 0> const target_y{};
    std::array<float, 0> const target_z{};

    ml::lerp_3d_in_place(current_x, current_y, current_z, target_x, target_y, target_z, .5f);
}
