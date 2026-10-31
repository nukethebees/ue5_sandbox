#include <ioj/arc.h>

#include <gtest/gtest.h>

TEST(NativeCoreArc, SubdivisionIncludesEndpointsAndEvenlySpacedInterior) {
    EXPECT_EQ(ioj::subdivide_arc_into_segments(0.f, 180.f, 1), (std::vector<float>{0.f, 180.f}));
    EXPECT_EQ(ioj::subdivide_arc_into_segments(-90.f, 180.f, 1), (std::vector<float>{-90.f, 90.f}));
    EXPECT_EQ(ioj::subdivide_arc_into_segments(-90.f, 180.f, 2),
              (std::vector<float>{-90.f, 0.f, 90.f}));
}
