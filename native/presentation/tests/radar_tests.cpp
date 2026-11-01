#include <ioj/presentation/radar.h>
#include <ioj/vectors3f.h>

#include <gtest/gtest.h>

#include <cmath>

namespace ioj::presentation {
namespace {
struct RadarScene {
    void add(Vector3f const position,
             Team const team,
             EntityType const type,
             sim::Health const health = sim::Health{100}) {
        auto const index{locations.add(position)};
        velocities.add({});
        ids.emplace_back(index, type);
        teams.push_back(team);
        types.push_back(type);
        healths.push_back(health);
        roles.push_back(ui::entity_overlay::ObjectiveRole::None);
    }

    auto collect(settings::Radar const& settings, Transform3d const& transform = {})
        -> RadarCollectionResult {
        std::vector<AgentDisplayBatch> batches;
        auto const count{locations.num()};
        batches.reserve(count);
        for (std::uint32_t index{}; index < count; ++index) {
            batches.push_back({types[index],
                               std::span{ids}.subspan(index, 1),
                               locations.const_view(index, 1),
                               velocities.const_view(index, 1),
                               sim::HealthConstView{types[index], healths},
                               std::span{teams}.subspan(index, 1)});
        }
        return collect_radar_instances(
            batches, roles, colours, transform, ids[0], Team::Blue, settings, frame);
    }

    Vectors3f locations;
    Vectors3f velocities;
    std::vector<sim::EntityId> ids;
    std::vector<Team> teams;
    std::vector<EntityType> types;
    std::vector<sim::Health> healths;
    std::vector<ui::entity_overlay::ObjectiveRole> roles;
    RadarContactColours colours;
    RadarFrame frame;
};

auto glyph(ui::radar::Instance const& instance) -> ui::radar::Glyph {
    return static_cast<ui::radar::Glyph>(instance.packed_glyph_and_flags & 0xffu);
}

auto flags(ui::radar::Instance const& instance) -> ui::radar::ContactFlags {
    return static_cast<ui::radar::ContactFlags>((instance.packed_glyph_and_flags >> 8u) & 0xffu);
}

auto linear_settings() -> settings::Radar {
    settings::Radar settings;
    settings.combat_range = 25.f;
    settings.tactical_range = 50.f;
    settings.maximum_range = 100.f;
    settings.combat_display_radius = 0.25f;
    return sanitize_radar_settings(settings);
}

auto nonlinear_settings() -> ioj::settings::Radar {
    ioj::settings::Radar settings;
    settings.combat_range = 100.0f;
    settings.tactical_range = 400.0f;
    settings.maximum_range = 2000.0f;
    settings.combat_display_radius = 0.45f;
    return ioj::presentation::sanitize_radar_settings(settings);
}

TEST(RadarSource, FiltersMapsAndOrdersContacts) {
    RadarScene scene;
    scene.add({}, Team::Blue, EntityType::PlayerShip);
    scene.add({50.f, 0.f, 0.f}, Team::Red, EntityType::Fighter);
    scene.add({0.f, 50.f, 25.f}, Team::Green, EntityType::CapitalShip);
    scene.add({-50.f, 0.f, 0.f}, Team::Yellow, EntityType::Turret);
    scene.add({0.f, -50.f, 0.f}, Team::White, EntityType::TubeSpinner);
    scene.add({100.f, 0.f, 0.f}, Team::Red, EntityType::Turret);
    scene.add({25.f, 0.f, 0.f}, Team::Red, EntityType::Fighter, sim::Health{});
    scene.add({101.f, 0.f, 0.f}, Team::Red, EntityType::CapitalShip);
    scene.roles[2] = ui::entity_overlay::ObjectiveRole::Defend;
    scene.roles[3] = ui::entity_overlay::ObjectiveRole::Destroy;
    scene.roles[7] = ui::entity_overlay::ObjectiveRole::Defend;

    auto const result{scene.collect(linear_settings())};
    EXPECT_EQ(result.candidate_count, 6);
    ASSERT_EQ(result.visible_count, 5);
    auto const& contacts{scene.frame.instances};
    EXPECT_EQ(glyph(contacts[0]), ui::radar::Glyph::Fighter);
    EXPECT_EQ(glyph(contacts[1]), ui::radar::Glyph::Unknown);
    EXPECT_EQ(glyph(contacts[2]), ui::radar::Glyph::CapitalShip);
    EXPECT_EQ(flags(contacts[2]), ui::radar::ContactFlags::DefendObjective);
    EXPECT_EQ(glyph(contacts[3]), ui::radar::Glyph::Turret);
    EXPECT_EQ(flags(contacts[3]), ui::radar::ContactFlags::DestroyObjective);
    EXPECT_EQ(glyph(contacts.back()), ui::radar::Glyph::Player);
    EXPECT_EQ(contacts.back().radar_position, Vector3f{});

    auto settings{linear_settings()};
    settings.maximum_range = 1.e-8f;
    settings.combat_range = 1.e-8f;
    settings.tactical_range = 1.e-8f;
    settings.combat_display_radius = 1.f;
    EXPECT_EQ(scene.collect(settings).visible_count, 1);
}

TEST(RadarSource, UsesPlayerRelativeContactColours) {
    RadarScene scene;
    scene.add({}, Team::Blue, EntityType::PlayerShip);
    scene.add({10.f, 0.f, 0.f}, Team::Blue, EntityType::Fighter);
    scene.add({20.f, 0.f, 0.f}, Team::Red, EntityType::CapitalShip);
    scene.add({30.f, 0.f, 0.f}, Team::White, EntityType::Turret);
    scene.colours = {.friendly = {0.f, 0.f, 1.f, 1.f},
                     .hostile = {1.f, 0.f, 0.f, 1.f},
                     .neutral = {1.f, 1.f, 1.f, 1.f}};
    static_cast<void>(scene.collect(linear_settings()));
    ASSERT_EQ(scene.frame.instances.size(), 4);
    EXPECT_EQ(scene.frame.instances[0].packed_color, 0xffff0000u);
    EXPECT_EQ(scene.frame.instances[1].packed_color, 0xff0000ffu);
    EXPECT_EQ(scene.frame.instances[2].packed_color, 0xffffffffu);
}

TEST(RadarSource, UsesYawAndPitchButRemovesRollFromContactsAndHeadings) {
    Transform3d const transform{.rotation = to_quaternion(Rotator3d{28., 37., 0.}),
                                .location = {1200., -500., 300.}};
    Vector3f const local_contact{50.f, 25.f, 20.f};
    RadarScene scene;
    scene.add(Vector3f{transform.location}, Team::Blue, EntityType::PlayerShip);
    scene.add(
        Vector3f{transform.location + transform.transform_vector_no_scale(Vector3d{local_contact})},
        Team::Red,
        EntityType::Fighter);
    set_vector(
        scene.velocities.view(), 1, Vector3f{transform.transform_vector_no_scale({0., 100., 0.})});
    static_cast<void>(scene.collect(linear_settings(), transform));
    ASSERT_EQ(scene.frame.instances.size(), 2);
    auto const contact{scene.frame.instances[0]};
    EXPECT_NEAR(contact.radar_position.z(), 0.2f, 1.e-4f);
    EXPECT_GT(contact.radar_position.x(), 0.f);
    EXPECT_GT(contact.radar_position.y(), 0.f);
    EXPECT_NEAR(static_cast<float>(contact.packed_glyph_and_flags >> 16u), 16384.f, 1.f);

    auto rolled{transform};
    rolled.rotation = to_quaternion(Rotator3d{28., 37., 120.});
    static_cast<void>(scene.collect(linear_settings(), rolled));
    ASSERT_EQ(scene.frame.instances.size(), 2);
    EXPECT_TRUE(
        almost_equal(scene.frame.instances[0].radar_position, contact.radar_position, 1.e-4f));
    EXPECT_EQ(scene.frame.instances[0].packed_glyph_and_flags, contact.packed_glyph_and_flags);
}

TEST(RadarSource, ContactMappingDoesNotDependOnOtherContacts) {
    RadarScene scene;
    scene.add({}, Team::Blue, EntityType::PlayerShip);
    scene.add({250.f, -80.f, 40.f}, Team::Red, EntityType::Fighter);
    auto const settings{nonlinear_settings()};
    static_cast<void>(scene.collect(settings));
    auto const original{scene.frame.instances[0].radar_position};
    scene.add({1500.f, 100.f, 0.f}, Team::Green, EntityType::CapitalShip);
    static_cast<void>(scene.collect(settings));
    ASSERT_EQ(scene.frame.instances.size(), 3);
    EXPECT_EQ(scene.frame.instances[0].radar_position, original);
    EXPECT_FLOAT_EQ(scene.frame.combat_display_radius, settings.combat_display_radius);
    EXPECT_NEAR(scene.frame.tactical_display_radius, 0.8f, 0.01f);
}

TEST(RadarSource, FixedMaximumRangeExcludesAllOutOfRangeContacts) {
    RadarScene scene;
    scene.add({}, Team::Blue, EntityType::PlayerShip);
    scene.add({1999.f, 0.f, 0.f}, Team::Red, EntityType::Fighter);
    scene.add({2000.f, 0.f, 0.f}, Team::Red, EntityType::CapitalShip);
    scene.add({1600.f, 0.f, 1200.f}, Team::Red, EntityType::Turret);
    scene.roles[2] = ui::entity_overlay::ObjectiveRole::Destroy;
    scene.roles[3] = ui::entity_overlay::ObjectiveRole::Defend;
    ASSERT_EQ(scene.collect(nonlinear_settings()).visible_count, 2);
    EXPECT_EQ(glyph(scene.frame.instances[0]), ui::radar::Glyph::Fighter);
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
