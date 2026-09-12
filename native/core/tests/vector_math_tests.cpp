#include <sandbox/core/vector_math.h>

#include <gtest/gtest.h>

#include <array>

TEST(NativeCoreVectorMath, ComputesScalarMetrics) {
    EXPECT_FLOAT_EQ(ml::native_math::size_squared(3.0f, 4.0f, 12.0f), 169.0f);
    EXPECT_FLOAT_EQ(ml::native_math::size(3.0f, 4.0f, 12.0f), 13.0f);
    EXPECT_FLOAT_EQ(ml::native_math::distance(0.0f, 0.0f, 0.0f, 3.0f, 4.0f, 12.0f), 13.0f);
    EXPECT_EQ(ml::native_math::dot_product(1, -1, 2, 2, 1, -1), -1);
}

TEST(NativeCoreVectorMath, AddsAndScalesSplitVectors) {
    std::array<float, 2> x{1.0f, 2.0f};
    std::array<float, 2> y{3.0f, 4.0f};
    std::array<float, 2> z{5.0f, 6.0f};
    std::array<float, 2> const sx{2.0f, 3.0f};
    std::array<float, 2> const sy{4.0f, 5.0f};
    std::array<float, 2> const sz{6.0f, 7.0f};

    ml::native_math::add_scaled_in_place(
        x.data(), y.data(), z.data(), sx.data(), sy.data(), sz.data(), 0.5f, 2);

    EXPECT_EQ(x, (std::array<float, 2>{2.0f, 3.5f}));
    EXPECT_EQ(y, (std::array<float, 2>{5.0f, 6.5f}));
    EXPECT_EQ(z, (std::array<float, 2>{8.0f, 9.5f}));
}

TEST(NativeCoreVectorMath, ProducesDirectionsDistancesAndRotations) {
    std::array<float, 2> const from_x{0.0f, 1.0f};
    std::array<float, 2> const from_y{0.0f, 1.0f};
    std::array<float, 2> const from_z{0.0f, 1.0f};
    std::array<float, 2> const to_x{3.0f, 1.0f};
    std::array<float, 2> const to_y{4.0f, 1.0f};
    std::array<float, 2> const to_z{0.0f, 1.0f};
    std::array<float, 2> out_x{};
    std::array<float, 2> out_y{};
    std::array<float, 2> out_z{};
    std::array<float, 2> distances{};

    ml::native_math::direction_and_distance(out_x.data(),
                                            out_y.data(),
                                            out_z.data(),
                                            distances.data(),
                                            from_x.data(),
                                            from_y.data(),
                                            from_z.data(),
                                            to_x.data(),
                                            to_y.data(),
                                            to_z.data(),
                                            2);

    EXPECT_FLOAT_EQ(out_x[0], 0.6f);
    EXPECT_FLOAT_EQ(out_y[0], 0.8f);
    EXPECT_FLOAT_EQ(distances[0], 5.0f);
    EXPECT_FLOAT_EQ(out_x[1], 0.0f);
    EXPECT_FLOAT_EQ(distances[1], 0.0f);

    std::array<float, 1> pitch{};
    std::array<float, 1> yaw{};
    std::array<float, 1> roll{};
    std::array<float, 1> const x{0.0f};
    std::array<float, 1> const y{1.0f};
    std::array<float, 1> const z{0.0f};
    ml::native_math::to_rotations(
        pitch.data(), yaw.data(), roll.data(), x.data(), y.data(), z.data(), 1);
    EXPECT_NEAR(yaw[0], 90.0f, 1.0e-5f);
    EXPECT_FLOAT_EQ(pitch[0], 0.0f);
    EXPECT_FLOAT_EQ(roll[0], 0.0f);
}
