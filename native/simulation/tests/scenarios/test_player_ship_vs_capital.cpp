#include "../support/simulation_test_support.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/testing/entity_observations.h>

namespace ioj::sim {
namespace player_vs_capital_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

TEST(WorldlessSpaceGameSimulation, PlayerShip_VersusCapital) {
    auto const config{tests::make_fixture()};
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = player_vs_capital_test::collision_resilient_health;
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 1.f;
    auto const player_index{tests::add_player_spawn(
        data,
        tests::make_player_spawn(config,
                                 Transform3d{.rotation = to_quaternion(Rotator3d{0.f, -90.f, 0.f}),
                                             .location = Vector3d{19850.f, 1300.f, 980.f}}))};
    data.player->health.health = player_vs_capital_test::collision_resilient_health;
    tests::add_capital_spawn(
        data, Vector3f{{-22020.f, 2170.f, 4360.f}}, Team::Green, player_index, 0.f, 120.f);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto* const player{harness.get_simulation().get_player_ship_simulation()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    assert(player);
    harness.get_simulation().get_player_ship_commands()->start_boost();
    auto const player_id{player->unique_entity_id};
    struct Sample {
        Vector3d player_location;
        Vector3d accessor_location;
        std::vector<Vector3f> fighter_target_locations{};
        std::vector<Vector3f> fighter_locations{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        Sample sample{.player_location = player->get_physical_state().transform.location,
                      .accessor_location = [&] {
                          auto const location{
                              observe_entity(harness.get_simulation(), player_id)->location};
                          return Vector3d{location.x(), location.y(), location.z()};
                      }()};
        sample.fighter_target_locations = tests::copy_vectors(fighters.get_target_locations());
        sample.fighter_locations = tests::copy_vectors(fighters.get_locations());
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.finish_at(5.6);
    ASSERT_TRUE(harness.run_until_timeline_finished(6.0))
        << "Player-versus-capital timeline completes";
    ASSERT_FALSE(samples.is_empty()) << "Player-versus-capital samples are recorded";

    auto const& settled{samples.nearest_value(0.1)};
    auto const& tracked{samples.nearest_value(0.6)};
    auto const& before_end{samples.nearest_value(5.1)};
    auto const& end{samples.nearest_value(5.6)};
    EXPECT_LE((size(settled.player_location - settled.accessor_location)), 1.0)
        << "Accessor and player locations match initially";
    EXPECT_LE((size(tracked.player_location - tracked.accessor_location)), 1.0)
        << "Accessor and player locations match after movement";
    EXPECT_GT((size(settled.player_location - tracked.player_location)), 1.0)
        << "Player ship moves";
    EXPECT_GT(static_cast<std::int32_t>(tracked.fighter_target_locations.size()), std::int32_t{0})
        << "Fighters have target locations";
    EXPECT_EQ(static_cast<std::int32_t>(tracked.fighter_target_locations.size()),
              static_cast<std::int32_t>(end.fighter_target_locations.size()))
        << "Fighter target count remains stable";
    EXPECT_EQ(static_cast<std::int32_t>(end.fighter_target_locations.size()),
              static_cast<std::int32_t>(end.fighter_locations.size()))
        << "Fighter and target counts match";
    if (::testing::Test::HasFailure()) {
        return;
    }
    auto const count{static_cast<std::int32_t>(end.fighter_locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        SCOPED_TRACE(::testing::Message() << "index " << i);
        EXPECT_GT(size(tracked.fighter_target_locations[i] - end.fighter_target_locations[i]), 1.f)
            << "Fighter target follows player";
        EXPECT_GT(size(before_end.fighter_locations[i] - end.fighter_locations[i]), 500.f)
            << "Fighter moves late in simulation";
        EXPECT_GT(size(before_end.fighter_target_locations[i] - end.fighter_target_locations[i]),
                  500.f)
            << "Fighter target updates late in simulation";
    }
}

}
