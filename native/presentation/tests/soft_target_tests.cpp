#include <ioj/presentation/soft_target.h>
#include <ioj/sim/fighters/fighter_entity_data.h>

#include <gtest/gtest.h>

namespace ioj::presentation {
namespace {
struct Entities {
    void add_defaulted(std::int32_t const count) {
        motion.add_defaulted(count);
        healths.resize(healths.size() + static_cast<std::size_t>(count));
        entity_ids.resize(entity_ids.size() + static_cast<std::size_t>(count));
        teams.resize(teams.size() + static_cast<std::size_t>(count));
        entity_types.resize(entity_types.size() + static_cast<std::size_t>(count));
    }
    auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>(healths.size()); }

    void add_health(std::int32_t const row, sim::EntityId const owner, sim::Health const health) {
        entity_ids[row] = owner;
        healths[row] = health;
    }

    void set_health(std::int32_t const row, sim::Health const health) { healths[row] = health; }

    sim::FighterEntityData motion;
    std::vector<sim::Health> healths;
    std::vector<sim::EntityId> entity_ids;
    std::vector<Team> teams;
    std::vector<sim::EntityType> entity_types;
};
using EntityTypeRadii = ::ioj::sim::EntityTypeRadii;

auto make_entity_type_radii() -> EntityTypeRadii {
    EntityTypeRadii radii{};
    for (auto const type : ioj::EnumTraits<::ioj::sim::EntityType>::values) {
        radii[type] = 1.0f;
    }
    return radii;
}

auto make_view(Entities const& entities) -> std::vector<::ioj::presentation::AgentDisplayBatch> {
    std::vector<::ioj::presentation::AgentDisplayBatch> batches;
    auto const count{entities.num()};
    batches.reserve(count);
    for (std::int32_t i{}; i < count; ++i) {
        batches.push_back({entities.entity_types[static_cast<std::size_t>(i)],
                           std::span{entities.entity_ids}.subspan(i, 1),
                           entities.motion.get_const_view(i, 1).view_locations(),
                           entities.motion.get_const_view(i, 1).view_velocities(),
                           ::ioj::sim::HealthConstView{std::span{entities.healths}.subspan(i, 1)},
                           std::span{entities.teams}.subspan(i, 1)});
    }
    return batches;
}

void add_entity(Entities& entities,
                EntityTypeRadii& entity_type_radii,
                Vector3f const position,
                std::int32_t const health,
                ::ioj::sim::EntityType const type,
                float const radius = 1.0f,
                bool const alive = true,
                Team const team = Team::White) {
    auto const index{entities.num()};
    entities.add_defaulted(1);
    ::ioj::set_vector(entities.motion.get_view().view_locations(), index, ioj::Vector3f{position});
    entity_type_radii[type] = radius;
    entities.teams[index] = team;
    entities.entity_types[index] = type;
    auto const owner{
        ::ioj::sim::EntityId(static_cast<std::uint32_t>(index), entities.entity_types[index])};
    entities.add_health(index, owner, alive ? health : 0);
}

auto make_forward_x_view() -> OverlayView {
    OverlayView view{.size = {1000.f, 1000.f}};
    view.view_projection[1][0] = 1.f;
    view.view_projection[2][1] = 1.f;
    view.view_projection[0][2] = 1.f;
    view.view_projection[0][3] = 1.f;
    return view;
}

auto entity_id(Entities const& entities, std::int32_t const index) -> ::ioj::sim::EntityId {
    auto const type{entities.entity_types[index]};
    return ::ioj::sim::EntityId(index, type);
}

auto select_target(Entities const& entities,
                   EntityTypeRadii const& entity_type_radii,
                   ::ioj::sim::EntityId const current_target = {},
                   float const weapon_range = 1000.0f) -> ioj::presentation::SoftTargetResult {
    std::vector<ui::entity_overlay::ObjectiveRole> objective_roles(entities.num());
    return select_soft_target(make_view(entities),
                              entity_type_radii,
                              objective_roles,
                              {.view = make_forward_x_view(),
                               .aim_origin = Vector3f{},
                               .aim_direction = {1.0f, 0.0f, 0.0f},
                               .camera_right = {0.0f, 1.0f, 0.0f},
                               .camera_up = {0.0f, 0.0f, 1.0f},
                               .player_team = Team::Green,
                               .effective_weapon_range = weapon_range,
                               .maximum_overlay_range = 10000.0f},
                              {},
                              current_target);
}
}

TEST(EntityOverlaySource, SelectsOnlyTheBestValidHostile) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Green);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 50.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Red);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 80.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Blue);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               false,
               Team::Red);

    auto const selected{select_target(entities, entity_type_radii)};
    EXPECT_EQ(selected.id.raw_value(), entity_id(entities, 1).raw_value())
        << "Nearest hostile is selected";
}

TEST(EntityOverlaySource, RetainsAndSwitchesWithHysteresis) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 65.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Red);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 80.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Blue);

    auto const retained{select_target(entities, entity_type_radii, entity_id(entities, 1))};
    EXPECT_EQ(retained.id.raw_value(), entity_id(entities, 1).raw_value())
        << "Small improvement does not switch";

    entities.motion.get_view().view_locations().ys()[0] = 50.0f;
    auto const switched{select_target(entities, entity_type_radii, entity_id(entities, 1))};
    EXPECT_EQ(switched.id.raw_value(), entity_id(entities, 0).raw_value())
        << "Materially better candidate switches";

    entities.motion.get_view().view_locations().ys()[0] = 250.0f;
    entities.motion.get_view().view_locations().ys()[1] = 250.0f;
    auto const cleared{select_target(entities, entity_type_radii, entity_id(entities, 1))};
    EXPECT_FALSE(cleared.id.is_valid()) << "Target outside retention is cleared";
    EXPECT_TRUE(cleared.previous_target_can_fade) << "Valid on-screen lost target may fade";

    entities.set_health(1, 0);
    auto const dead{select_target(entities, entity_type_radii, entity_id(entities, 1))};
    EXPECT_FALSE(dead.previous_target_can_fade) << "Dead target may not fade";
}

TEST(EntityOverlaySource, ProjectedSizeDoesNotLetARearTargetDominate) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {2000.0f, 80.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::CapitalShip,
               800.0f,
               true,
               Team::Red);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 20.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               5.0f,
               true,
               Team::Blue);

    auto const selected{select_target(entities, entity_type_radii)};
    EXPECT_EQ(selected.id.raw_value(), entity_id(entities, 1).raw_value())
        << "Better-centred front target wins";
}

TEST(EntityOverlaySource, NearestSurfaceResolvesCentredTargetsAndHysteresis) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {2000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::CapitalShip,
               100.0f,
               true,
               Team::Red);
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               5.0f,
               true,
               Team::Blue);

    auto const selected{select_target(entities, entity_type_radii)};
    EXPECT_EQ(selected.id.raw_value(), entity_id(entities, 1).raw_value())
        << "Nearest tied centre wins";

    auto const switched{select_target(entities, entity_type_radii, entity_id(entities, 0))};
    EXPECT_EQ(switched.id.raw_value(), entity_id(entities, 1).raw_value())
        << "Materially nearer tied target replaces rear target";
}

TEST(EntityOverlaySource, NormalizesRangeAlphaUsingTargetSurfaceDistance) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {5000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               0.0f,
               true,
               Team::Red);

    auto const comfortably_out_of_range{select_target(entities, entity_type_radii)};
    EXPECT_EQ(comfortably_out_of_range.range_alpha, 0.0f)
        << "Beyond transition start clamps to zero";

    entities.motion.get_view().view_locations().xs()[0] = 2500.0f;
    auto const approaching{select_target(entities, entity_type_radii)};
    EXPECT_EQ(approaching.range_alpha, 0.5f);

    entities.motion.get_view().view_locations().xs()[0] = 1100.0f;
    entity_type_radii[::ioj::sim::EntityType::Turret] = 100.0f;
    auto const at_range_boundary{select_target(entities, entity_type_radii)};
    EXPECT_EQ(at_range_boundary.range_alpha, 1.0f)
        << "Surface at range boundary has full range alpha";

    entities.motion.get_view().view_locations().xs()[0] = 500.0f;
    auto const inside_range{select_target(entities, entity_type_radii)};
    EXPECT_EQ(inside_range.range_alpha, 1.0f);

    auto const invalid_weapon_range{select_target(entities, entity_type_radii, {}, 0.0f)};
    EXPECT_EQ(invalid_weapon_range.range_alpha, 0.0f)
        << "Missing weapon range produces zero range alpha";
}

TEST(EntityOverlaySource, RangeAlphaUsesTargetSurfaceRatherThanCentre) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {1100.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               100.0f,
               true,
               Team::Red);

    auto const selected{select_target(entities, entity_type_radii)};
    EXPECT_EQ(selected.range_alpha, 1.0f) << "Target surface at weapon range has full range alpha";
}

TEST(EntityOverlaySource, ReportsWorldScaleSeparatelyFromClampedIndicatorRadius) {
    Entities entities;
    auto entity_type_radii{make_entity_type_radii()};
    add_entity(entities,
               entity_type_radii,
               {1000.0f, 0.0f, 0.0f},
               20,
               ::ioj::sim::EntityType::Turret,
               2.0f,
               true,
               Team::Red);

    auto const distant{select_target(entities, entity_type_radii)};
    EXPECT_TRUE(distant.world_units_per_pixel > 0.0f)
        << "Projection scale is available for world marker scaling";
    EXPECT_TRUE(distant.indicator_radius_pixels > 2.0f / distant.world_units_per_pixel)
        << "Small projected targets still use the minimum indicator";

    entity_type_radii[::ioj::sim::EntityType::Turret] = 0.0f;
    auto const zero_radius{select_target(entities, entity_type_radii)};
    EXPECT_TRUE(zero_radius.world_units_per_pixel > 0.0f)
        << "Zero-radius targets still have a usable projection scale";

    entities.motion.get_view().view_locations().xs()[0] = 500.0f;
    auto const nearer{select_target(entities, entity_type_radii)};
    EXPECT_TRUE(nearer.world_units_per_pixel < distant.world_units_per_pixel)
        << "World units per pixel shrink as the target approaches";
}

TEST(EntityOverlaySource, TiesUseEntityIdentityRegardlessOfBatchOrdering) {
    Entities entities;
    auto radii{make_entity_type_radii()};
    for (int index{}; index < 2; ++index) {
        add_entity(
            entities, radii, {1000.f, 0.f, 0.f}, 20, EntityType::Turret, 0.f, true, Team::Red);
    }
    auto batches{make_view(entities)};
    std::array<ui::entity_overlay::ObjectiveRole, 2> roles{};
    SoftTargetContext context{.view = make_forward_x_view(), .maximum_overlay_range = 10000.f};
    auto const first{select_soft_target(batches, radii, roles, context, {}, {})};
    std::ranges::reverse(batches);
    auto const reversed{select_soft_target(batches, radii, roles, context, {}, {})};
    EXPECT_EQ(first.id, entity_id(entities, 0));
    EXPECT_EQ(reversed.id, first.id);
}

TEST(EntityOverlaySource, ObjectivesBypassRangeButNotProjectionOrHealth) {
    Entities entities;
    auto radii{make_entity_type_radii()};
    add_entity(entities, radii, {1000.f, 0.f, 0.f}, 20, EntityType::Turret, 0.f, true, Team::Red);
    std::array<ui::entity_overlay::ObjectiveRole, 1> roles{};
    SoftTargetContext context{.view = make_forward_x_view(), .maximum_overlay_range = 500.f};
    auto const select{
        [&] { return select_soft_target(make_view(entities), radii, roles, context, {}, {}); }};
    EXPECT_FALSE(select().id.is_valid());
    roles[0] = ui::entity_overlay::ObjectiveRole::Destroy;
    EXPECT_EQ(select().id, entity_id(entities, 0));
    entities.motion.get_view().view_locations().ys()[0] = 2000.f;
    EXPECT_FALSE(select().id.is_valid());
    entities.motion.get_view().view_locations().ys()[0] = 0.f;
    context.view.view_projection[0][3] = 0.f;
    EXPECT_FALSE(select().id.is_valid());
    context.view = make_forward_x_view();
    context.aim_direction = {};
    EXPECT_FALSE(select().id.is_valid());
    context.aim_direction = {1.f, 0.f, 0.f};
    entities.set_health(0, 0);
    EXPECT_FALSE(select().id.is_valid());
}

}
