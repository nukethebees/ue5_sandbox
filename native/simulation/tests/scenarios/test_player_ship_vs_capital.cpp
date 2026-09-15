#include "test_player_ship_vs_capital.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/fighters/sim.h>

namespace ioj::sim {
namespace player_vs_capital_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_player_ship_vs_capital(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = player_vs_capital_test::collision_resilient_health;
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 1.f;
    auto const player_index{tests::add_player_spawn(
        data,
        tests::make_player_spawn(config,
                                 Transform3d{.rotation = to_quaternion(Rotator3d{0.f, -90.f, 0.f}),
                                             .location = ml::Vector3d{19850.f, 1300.f, 980.f}}))};
    data.player->health.health = player_vs_capital_test::collision_resilient_health;
    tests::add_capital_spawn(
        data, Vector3f{{-22020.f, 2170.f, 4360.f}}, Team::Green, player_index, 0.f, 120.f);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto* const player{harness.get_simulation().get_player_ship_simulation()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    assert(player);
    harness.get_simulation().get_player_ship_commands()->set_flight_mode(
        SpaceShipFlightMode::ForwardSpeed);
    harness.get_simulation().get_player_ship_commands()->start_boost();
    auto const player_handle{player->registry_handle};
    struct Sample {
        ml::Vector3d player_location;
        ml::Vector3d registry_location;
        std::vector<Vector3f> fighter_target_locations{};
        std::vector<Vector3f> fighter_locations{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        Sample sample{.player_location = player->transform.location, .registry_location = [&] {
                          auto const location{harness.get_registry().get_location(player_handle)};
                          return ml::Vector3d{location.X, location.Y, location.Z};
                      }()};
        sample.fighter_target_locations = tests::copy_vectors(fighters.get_target_locations());
        sample.fighter_locations = tests::copy_vectors(fighters.get_locations());
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.finish_at(5.6);
    tests::expect_true(harness.run_until_timeline_finished(6.0),
                       "Player-versus-capital timeline completes");
    tests::expect_true(!samples.is_empty(), "Player-versus-capital samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& settled{samples.nearest_value(0.1)};
    auto const& tracked{samples.nearest_value(0.6)};
    auto const& before_end{samples.nearest_value(5.1)};
    auto const& end{samples.nearest_value(5.6)};
    tests::expect_distance_near(settled.player_location,
                                settled.registry_location,
                                1.0,
                                "Registry and player locations match initially");
    tests::expect_distance_near(tracked.player_location,
                                tracked.registry_location,
                                1.0,
                                "Registry and player locations match after movement");
    tests::expect_distance_not_near(
        settled.player_location, tracked.player_location, 1.0, "Player ship moves");
    tests::expect_greater(static_cast<std::int32_t>(tracked.fighter_target_locations.size()),
                          std::int32_t{0},
                          "Fighters have target locations");
    tests::expect_equal(static_cast<std::int32_t>(tracked.fighter_target_locations.size()),
                        static_cast<std::int32_t>(end.fighter_target_locations.size()),
                        "Fighter target count remains stable");
    tests::expect_equal(static_cast<std::int32_t>(end.fighter_target_locations.size()),
                        static_cast<std::int32_t>(end.fighter_locations.size()),
                        "Fighter and target counts match");
    if (::testing::Test::HasFailure()) {
        return;
    }
    auto const count{static_cast<std::int32_t>(end.fighter_locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        tests::expect_distance_not_near(tracked.fighter_target_locations[i],
                                        end.fighter_target_locations[i],
                                        1.f,
                                        "Fighter target follows player",
                                        i);
        tests::expect_distance_greater(before_end.fighter_locations[i],
                                       end.fighter_locations[i],
                                       500.f,
                                       "Fighter moves late in simulation",
                                       i);
        tests::expect_distance_greater(before_end.fighter_target_locations[i],
                                       end.fighter_target_locations[i],
                                       500.f,
                                       "Fighter target updates late in simulation",
                                       i);
    }
}

}
