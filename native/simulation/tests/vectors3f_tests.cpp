#include "ioj/sim/vector_operations.h"

#include <gtest/gtest.h>

namespace ioj::sim {
TEST(Vectors3f, VectorOperationsUseNativeOwnersAndViews) {
    Vectors3f vectors;
    vectors.add(1.0f, 2.0f, 3.0f);
    vectors.add(4.0f, 5.0f, 6.0f);

    multiply_in_place(vectors.get_view(), 2.0f);

    EXPECT_FLOAT_EQ(vectors.xs[0], 2.0f);
    EXPECT_FLOAT_EQ(vectors.ys[0], 4.0f);
    EXPECT_FLOAT_EQ(vectors.zs[0], 6.0f);
    EXPECT_FLOAT_EQ(vectors.xs[1], 8.0f);
    EXPECT_FLOAT_EQ(vectors.ys[1], 10.0f);
    EXPECT_FLOAT_EQ(vectors.zs[1], 12.0f);
}

TEST(Vectors3f, ConvertsDirectionsToNativeRotators) {
    Vectors3f directions;
    directions.add(1.0f, 0.0f, 0.0f);
    directions.add(0.0f, 1.0f, 0.0f);

    Rotators3f rotations;
    to_rotations(rotations, directions.get_const_view());

    EXPECT_EQ(rotations.num(), 2);
    EXPECT_FLOAT_EQ(rotations.yaws[0], 0.0f);
    EXPECT_FLOAT_EQ(rotations.yaws[1], 90.0f);
}
} // namespace ioj::sim
