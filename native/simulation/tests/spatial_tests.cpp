#include "ioj/sim/collision_grid.h"
#include "ioj/sim/spatial.h"
#include "ioj/sim/trace_hits.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace ioj::sim::tests {

TEST(Spatial, SamplesSphericalShellSquaredDistanceUniformly) {
    auto const minimum{
        sample_spherical_shell_point({1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.0f)};
    auto const maximum{
        sample_spherical_shell_point({1.0, 2.0, 3.0}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 1.0f)};
    auto const midpoint{sample_spherical_shell_point({}, {1.0, 0.0, 0.0}, 10.0f, 20.0f, 0.5f)};

    EXPECT_DOUBLE_EQ(minimum.x, 11.0);
    EXPECT_DOUBLE_EQ(maximum.x, 21.0);
    EXPECT_DOUBLE_EQ(minimum.y, 2.0);
    EXPECT_NEAR(midpoint.x, std::sqrt(250.0), 1e-6);
}

TEST(Spatial, ClampsUnitSample) {
    auto const below{sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, -1.0f)};
    auto const above{sample_spherical_shell_point({}, {0.0, 1.0, 0.0}, 2.0f, 4.0f, 2.0f)};

    EXPECT_DOUBLE_EQ(below.y, 2.0);
    EXPECT_DOUBLE_EQ(above.y, 4.0);
}

TEST(Spatial, AppliesCentreAndDirection) {
    auto const point{
        sample_spherical_shell_point({1.0, 2.0, 3.0}, {0.0, 0.6, 0.8}, 3.0f, 5.0f, 1.0f)};

    EXPECT_DOUBLE_EQ(point.x, 1.0);
    EXPECT_DOUBLE_EQ(point.y, 5.0);
    EXPECT_DOUBLE_EQ(point.z, 7.0);
}

TEST(CollisionGrid, ValidatesDimensionsCellSizeAndCellCount) {
    EXPECT_TRUE(collision::is_configured({{2, 3, 4}, Vector3f{{5000.0f, 5000.0f, 20000.0f}}}));
    EXPECT_FALSE(collision::is_configured({{0, 3, 4}, Vector3f{{5000.0f, 5000.0f, 20000.0f}}}));
    EXPECT_FALSE(collision::is_configured({{2, 3, 4}, Vector3f{{5000.0f, 0.0f, 20000.0f}}}));
    EXPECT_FALSE(collision::is_configured({{2, 3, 4}, Vector3f{{5000.0f, -1.0f, 20000.0f}}}));
    EXPECT_FALSE(collision::is_configured(
        {{2, 3, 4}, Vector3f{{std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.0f}}}));
    EXPECT_FALSE(collision::is_configured(
        {{2, 3, 4}, Vector3f{{std::numeric_limits<float>::infinity(), 1.0f, 1.0f}}}));
    EXPECT_FALSE(collision::is_configured(
        {{2, 3, 4}, Vector3f{{-std::numeric_limits<float>::infinity(), 1.0f, 1.0f}}}));
    EXPECT_FALSE(collision::is_configured(
        {{std::numeric_limits<int>::max(), 2, 1}, Vector3f{{1.0f, 1.0f, 1.0f}}}));
}

TEST(NativeSoa, SwapRemovalKeepsTraceHitColumnsAligned) {
    TraceHits hits;
    for (std::int32_t index{}; index < 6; ++index) {
        hits.add(Vector3f{{static_cast<float>(index),
                           static_cast<float>(index + 10),
                           static_cast<float>(index + 20)}},
                 EntityUniqueId{static_cast<std::uint32_t>(index + 100)},
                 index + 200,
                 static_cast<std::uint8_t>(index + 1));
    }

    hits.remove_at_swap(2, 3);

    ASSERT_EQ(hits.num(), 3);
    auto const columns{hits.get_const_view()};
    EXPECT_EQ(columns.locations.xs[0], 0.0f);
    EXPECT_EQ(columns.locations.xs[1], 1.0f);
    EXPECT_EQ(columns.locations.xs[2], 5.0f);
    EXPECT_EQ(columns.entities[2], EntityUniqueId{105});
    EXPECT_EQ(columns.static_geometry_indices[2], 205);
    EXPECT_EQ(columns.hits[2], 6);
}

TEST(CollisionGrid, CalculatesDimensionsFromWorldAndCellSizes) {
    EXPECT_EQ(collision::calculate_grid_dimensions(Vector3f{{2000000.0f, 2000000.0f, 100000.0f}},
                                                   Vector3f{{5000.0f, 5000.0f, 20000.0f}}),
              (collision::CellCoord{400, 400, 5}));
    EXPECT_EQ(collision::calculate_grid_dimensions(Vector3f{{10.1f, 20.0f, 30.0f}},
                                                   Vector3f{{3.0f, 4.0f, 5.0f}}),
              (collision::CellCoord{4, 5, 6}));
    EXPECT_EQ(collision::calculate_grid_dimensions(Vector3f{{0.0f, 10.0f, 10.0f}},
                                                   Vector3f{{1.0f, 1.0f, 1.0f}}),
              (collision::CellCoord{0, 10, 10}));
    EXPECT_EQ(collision::calculate_grid_dimensions(
                  Vector3f{{std::numeric_limits<float>::max(), 1.0f, 1.0f}},
                  Vector3f{{std::numeric_limits<float>::denorm_min(), 1.0f, 1.0f}}),
              (collision::CellCoord{0, 1, 1}));
}

} // namespace tests
