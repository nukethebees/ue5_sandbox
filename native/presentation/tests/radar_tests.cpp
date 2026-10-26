#include <ioj/presentation/radar.h>

#include <gtest/gtest.h>

#include <cmath>

namespace ioj::presentation {
namespace {
auto nonlinear_settings() -> ioj::settings::Radar {
    ioj::settings::Radar settings;
    settings.combat_range = 100.0f;
    settings.tactical_range = 400.0f;
    settings.maximum_range = 2000.0f;
    settings.combat_display_radius = 0.45f;
    return ioj::presentation::sanitize_radar_settings(settings);
}
}
TEST(RadarSource, NonlinearMappingUsesStableSemanticAnchors) {
    auto const settings{nonlinear_settings()};
    auto const centre{radar::to_radar_position(Vector3f{}, settings)};
    auto const combat{radar::to_radar_position({settings.combat_range, 0.0f, 0.0f}, settings)};
    auto const maximum{radar::to_radar_position({settings.maximum_range, 0.0f, 0.0f}, settings)};

    EXPECT_EQ(centre, Vector3f{}) << "The centre maps exactly to the origin";
    EXPECT_NEAR(combat.x(), settings.combat_display_radius, 1.e-4f);
    EXPECT_NEAR(maximum.x(), 1.0f, 1.e-4f) << "The maximum range maps exactly to the edge";
    EXPECT_NEAR(radar::to_radar_display_radius(settings.tactical_range, settings), 0.8f, 0.01f)
        << "The tactical boundary maps near eighty percent";

    auto previous_radius{0.0f};
    for (int distance{1}; distance <= 2000; ++distance) {
        SCOPED_TRACE(::testing::Message() << "distance=" << distance);
        auto const radius{radar::to_radar_display_radius(static_cast<float>(distance), settings)};
        EXPECT_GT(radius, previous_radius) << "Mapping is strictly monotonic";
        previous_radius = radius;
    }

    for (auto const boundary : {settings.combat_range, settings.tactical_range}) {
        SCOPED_TRACE(::testing::Message() << "boundary=" << boundary);
        auto const before{radar::to_radar_display_radius(boundary - 1.0f, settings)};
        auto const at{radar::to_radar_display_radius(boundary, settings)};
        auto const after{radar::to_radar_display_radius(boundary + 1.0f, settings)};
        EXPECT_TRUE(std::isfinite(before)) << "Movement remains finite before a semantic boundary";
        EXPECT_TRUE(std::isfinite(at)) << "Movement remains finite at a semantic boundary";
        EXPECT_TRUE(std::isfinite(after)) << "Movement remains finite after a semantic boundary";
        EXPECT_NEAR(at - before, after - at, 0.0001f)
            << "Movement remains smooth across a semantic boundary";
    }
}

TEST(RadarSource, NonlinearMappingPreservesBearingAndCompressesHeight) {
    auto const settings{nonlinear_settings()};
    Vector2f const planar_deltas[]{
        {80.0f, 60.0f}, {-80.0f, 60.0f}, {-80.0f, -60.0f}, {80.0f, -60.0f}};
    for (auto const planar_delta : planar_deltas) {
        SCOPED_TRACE(::testing::Message()
                     << "planar_delta=(" << planar_delta.x() << ", " << planar_delta.y() << ')');
        auto const mapped{
            radar::to_radar_position({planar_delta.x(), planar_delta.y(), 0.0f}, settings)};
        auto const cross{planar_delta.x() * mapped.y() - planar_delta.y() * mapped.x()};
        auto const dot{planar_delta.x() * mapped.x() + planar_delta.y() * mapped.y()};
        EXPECT_NEAR(cross, 0.f, 0.001f) << "Planar bearing is preserved in every quadrant";
        EXPECT_GT(dot, 0.0f) << "Planar bearing points in the original direction";
    }

    auto previous_height{0.0f};
    for (float const height : {25.0f, 100.0f, 400.0f, 1000.0f}) {
        SCOPED_TRACE(::testing::Message() << "height=" << height);
        auto const above{radar::to_radar_position({0.0f, 0.0f, height}, settings).z()};
        auto const below{radar::to_radar_position({0.0f, 0.0f, -height}, settings).z()};
        EXPECT_GT(above, 0.0f) << "Vertical compression preserves the sign";
        EXPECT_NEAR(above, -below, 1.e-4f) << "Vertical compression is symmetric";
        EXPECT_GT(above, previous_height) << "Vertical compression is strictly monotonic";
        previous_height = above;
    }
}

}
