#include "ioj/vector_operations.h"

#include <gtest/gtest.h>

namespace ioj::sim {
TEST(Vectors3f, VectorOperationsUseNativeOwnersAndViews) {
    Vectors3f vectors;
    vectors.add(1.0f, 2.0f, 3.0f);
    vectors.add(4.0f, 5.0f, 6.0f);

    multiply_in_place(vectors.get_view().resolve(), 2.0f);

    auto const values{vectors.get_const_view().resolve()};
    EXPECT_EQ(values[0], (Vector3f{2.0f, 4.0f, 6.0f}));
    EXPECT_EQ(values[1], (Vector3f{8.0f, 10.0f, 12.0f}));
}

TEST(Vectors3f, ConvertsDirectionsToNativeRotators) {
    Vectors3f directions;
    directions.add(1.0f, 0.0f, 0.0f);
    directions.add(0.0f, 1.0f, 0.0f);

    Rotators3f rotations;
    to_rotations(rotations, directions.get_const_view().resolve());

    EXPECT_EQ(rotations.num(), 2);
    EXPECT_FLOAT_EQ(rotations.yaws()[0], 0.0f);
    EXPECT_FLOAT_EQ(rotations.yaws()[1], 90.0f);
}
} // namespace ioj::sim
