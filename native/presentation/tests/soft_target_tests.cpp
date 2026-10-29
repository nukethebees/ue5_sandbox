#include <ioj/presentation/soft_target.h>
#include <ioj/vectors3f.h>

#include <gtest/gtest.h>

namespace ioj::presentation {
namespace {
struct Entities {
    auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>(healths.size()); }

    Vectors3f positions;
    std::vector<sim::Health> healths;
    std::vector<sim::EntityId> entity_ids;
    std::vector<Team> teams;
    std::vector<sim::EntityType> entity_types;
};
using EntityTypeRadii = ::ioj::sim::EntityTypeRadii;

auto make_view(Entities const& entities) -> std::vector<::ioj::presentation::AgentDisplayBatch> {
    std::vector<::ioj::presentation::AgentDisplayBatch> batches;
    auto const count{entities.num()};
    batches.reserve(count);
    for (std::int32_t i{}; i < count; ++i) {
        batches.push_back({entities.entity_types[static_cast<std::size_t>(i)],
                           std::span{entities.entity_ids}.subspan(i, 1),
                           entities.positions.get_const_view(i, 1),
                           {},
                           ::ioj::sim::HealthConstView{std::span{entities.healths}.subspan(i, 1)},
                           std::span{entities.teams}.subspan(i, 1)});
    }
    return batches;
}

struct EntityInput {
    Vector3f position{};
    sim::Health health{20};
    sim::EntityType type{sim::EntityType::Turret};
    float radius{};
    Team team{Team::Red};
};

auto make_forward_x_view() -> OverlayView {
    OverlayView view{.size = {1000.f, 1000.f}};
    view.view_projection[1][0] = 1.f;
    view.view_projection[2][1] = 1.f;
    view.view_projection[0][2] = 1.f;
    view.view_projection[0][3] = 1.f;
    return view;
}

class EntityOverlaySource : public testing::Test {
  protected:
    void add_entity(EntityInput const input) {
        auto const index{entities_.positions.add(input.position)};
        entities_.healths.push_back(input.health);
        entities_.entity_ids.emplace_back(index, input.type);
        entities_.teams.push_back(input.team);
        entities_.entity_types.push_back(input.type);
        radii_[input.type] = input.radius;
    }

    auto entity_id(std::int32_t const index) const -> sim::EntityId {
        return sim::EntityId(index, entities_.entity_types[index]);
    }

    auto select_target(sim::EntityId const current_target = {},
                       float const weapon_range = 1000.0f) const -> SoftTargetResult {
        std::vector<ui::entity_overlay::ObjectiveRole> objective_roles(entities_.num());
        return select_soft_target(make_view(entities_),
                                  radii_,
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

    Entities entities_;
    EntityTypeRadii radii_{1.0f};
};
}

TEST_F(EntityOverlaySource, SelectsOnlyTheBestValidHostile) {
    add_entity({.position = {1000.0f, 0.0f, 0.0f}, .team = Team::Green});
    add_entity({.position = {1000.0f, 50.0f, 0.0f}});
    add_entity({.position = {1000.0f, 80.0f, 0.0f}, .team = Team::Blue});
    add_entity({.position = {1000.0f, 0.0f, 0.0f}, .health = sim::Health{0}});

    auto const selected{select_target()};
    EXPECT_EQ(selected.id.raw_value(), entity_id(1).raw_value()) << "Nearest hostile is selected";
}

TEST_F(EntityOverlaySource, RetainsAndSwitchesWithHysteresis) {
    add_entity({.position = {1000.0f, 65.0f, 0.0f}});
    add_entity({.position = {1000.0f, 80.0f, 0.0f}, .team = Team::Blue});

    auto const retained{select_target(entity_id(1))};
    EXPECT_EQ(retained.id.raw_value(), entity_id(1).raw_value())
        << "Small improvement does not switch";

    entities_.positions.get_view().ys()[0] = 50.0f;
    auto const switched{select_target(entity_id(1))};
    EXPECT_EQ(switched.id.raw_value(), entity_id(0).raw_value())
        << "Materially better candidate switches";

    entities_.positions.get_view().ys()[0] = 250.0f;
    entities_.positions.get_view().ys()[1] = 250.0f;
    auto const cleared{select_target(entity_id(1))};
    EXPECT_FALSE(cleared.id.is_valid()) << "Target outside retention is cleared";
    EXPECT_TRUE(cleared.previous_target_can_fade) << "Valid on-screen lost target may fade";

    entities_.healths[1] = sim::Health{};
    auto const dead{select_target(entity_id(1))};
    EXPECT_FALSE(dead.previous_target_can_fade) << "Dead target may not fade";
}

TEST_F(EntityOverlaySource, ProjectedSizeDoesNotLetARearTargetDominate) {
    add_entity(
        {.position = {2000.0f, 80.0f, 0.0f}, .type = EntityType::CapitalShip, .radius = 800.0f});
    add_entity({.position = {1000.0f, 20.0f, 0.0f}, .radius = 5.0f, .team = Team::Blue});

    auto const selected{select_target()};
    EXPECT_EQ(selected.id.raw_value(), entity_id(1).raw_value())
        << "Better-centred front target wins";
}

TEST_F(EntityOverlaySource, NearestSurfaceResolvesCentredTargetsAndHysteresis) {
    add_entity(
        {.position = {2000.0f, 0.0f, 0.0f}, .type = EntityType::CapitalShip, .radius = 100.0f});
    add_entity({.position = {1000.0f, 0.0f, 0.0f}, .radius = 5.0f, .team = Team::Blue});

    auto const selected{select_target()};
    EXPECT_EQ(selected.id.raw_value(), entity_id(1).raw_value()) << "Nearest tied centre wins";

    auto const switched{select_target(entity_id(0))};
    EXPECT_EQ(switched.id.raw_value(), entity_id(1).raw_value())
        << "Materially nearer tied target replaces rear target";
}

TEST_F(EntityOverlaySource, NormalizesRangeAlphaUsingTargetSurfaceDistance) {
    add_entity({.position = {5000.0f, 0.0f, 0.0f}});

    auto const comfortably_out_of_range{select_target()};
    EXPECT_EQ(comfortably_out_of_range.range_alpha, 0.0f)
        << "Beyond transition start clamps to zero";

    entities_.positions.get_view().xs()[0] = 2500.0f;
    auto const approaching{select_target()};
    EXPECT_EQ(approaching.range_alpha, 0.5f);

    entities_.positions.get_view().xs()[0] = 1100.0f;
    radii_[sim::EntityType::Turret] = 100.0f;
    auto const at_range_boundary{select_target()};
    EXPECT_EQ(at_range_boundary.range_alpha, 1.0f)
        << "Surface at range boundary has full range alpha";

    entities_.positions.get_view().xs()[0] = 500.0f;
    auto const inside_range{select_target()};
    EXPECT_EQ(inside_range.range_alpha, 1.0f);

    auto const invalid_weapon_range{select_target({}, 0.0f)};
    EXPECT_EQ(invalid_weapon_range.range_alpha, 0.0f)
        << "Missing weapon range produces zero range alpha";
}

TEST_F(EntityOverlaySource, ReportsWorldScaleSeparatelyFromClampedIndicatorRadius) {
    add_entity({.position = {1000.0f, 0.0f, 0.0f}, .radius = 2.0f});

    auto const distant{select_target()};
    EXPECT_TRUE(distant.world_units_per_pixel > 0.0f)
        << "Projection scale is available for world marker scaling";
    EXPECT_TRUE(distant.indicator_radius_pixels > 2.0f / distant.world_units_per_pixel)
        << "Small projected targets still use the minimum indicator";

    radii_[sim::EntityType::Turret] = 0.0f;
    auto const zero_radius{select_target()};
    EXPECT_TRUE(zero_radius.world_units_per_pixel > 0.0f)
        << "Zero-radius targets still have a usable projection scale";

    entities_.positions.get_view().xs()[0] = 500.0f;
    auto const nearer{select_target()};
    EXPECT_TRUE(nearer.world_units_per_pixel < distant.world_units_per_pixel)
        << "World units per pixel shrink as the target approaches";
}

TEST_F(EntityOverlaySource, TiesUseEntityIdentityRegardlessOfBatchOrdering) {
    for (int index{}; index < 2; ++index) {
        add_entity({.position = {1000.f, 0.f, 0.f}});
    }
    auto batches{make_view(entities_)};
    std::array<ui::entity_overlay::ObjectiveRole, 2> roles{};
    SoftTargetContext context{.view = make_forward_x_view(), .maximum_overlay_range = 10000.f};
    auto const first{select_soft_target(batches, radii_, roles, context, {}, {})};
    std::ranges::reverse(batches);
    auto const reversed{select_soft_target(batches, radii_, roles, context, {}, {})};
    EXPECT_EQ(first.id, entity_id(0));
    EXPECT_EQ(reversed.id, first.id);
}

TEST_F(EntityOverlaySource, ObjectivesBypassRangeButNotProjectionOrHealth) {
    add_entity({.position = {1000.f, 0.f, 0.f}});
    std::array<ui::entity_overlay::ObjectiveRole, 1> roles{};
    SoftTargetContext context{.view = make_forward_x_view(), .maximum_overlay_range = 500.f};
    auto const select{
        [&] { return select_soft_target(make_view(entities_), radii_, roles, context, {}, {}); }};
    EXPECT_FALSE(select().id.is_valid());
    roles[0] = ui::entity_overlay::ObjectiveRole::Destroy;
    EXPECT_EQ(select().id, entity_id(0));
    entities_.positions.get_view().ys()[0] = 2000.f;
    EXPECT_FALSE(select().id.is_valid());
    entities_.positions.get_view().ys()[0] = 0.f;
    context.view.view_projection[0][3] = 0.f;
    EXPECT_FALSE(select().id.is_valid());
    context.view = make_forward_x_view();
    context.aim_direction = {};
    EXPECT_FALSE(select().id.is_valid());
    context.aim_direction = {1.f, 0.f, 0.f};
    entities_.healths[0] = sim::Health{};
    EXPECT_FALSE(select().id.is_valid());
}

}
