#include "support/simulation_test_support.h"
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include <ioj/testing/math_diff.h>
#include <ioj/testing/structural_diff.h>
#include <ioj/testing/vector_comparison.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory_resource>
#include <optional>
#include <utility>

namespace ioj::sim::tests {
namespace player_flight_tests {
auto make_data(Rotator3d const rotation = {}) -> SimulationScenario {
    SimulationScenario data;
    data.runtime.collision.grid_spec = {{16, 16, 4}, {{100000.f, 100000.f, 100000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(16);
    data.runtime.tick_memory_capacity_bytes = 1024 * 1024;
    data.runtime.phase_memory_capacity_bytes = 1024 * 1024;
    data.runtime.clock_settings.tick_rate = ioj::Frequency::make(60.0);
    for (auto const type : EnumTraits<EntityType>::values) {
        data.runtime.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }

    player::PlayerSpawnData spawn{};
    spawn.transform.rotation = to_quaternion(rotation);
    add_player_spawn(data, spawn);
    return data;
}

void advance_ticks(LevelSim& simulation, std::int32_t const count) {
    auto const dt{simulation.clock().tick_period()};
    for (std::int32_t tick{}; tick < count; ++tick) {
        simulation.advance(dt);
    }
}

void start(LevelSim& simulation) {

    simulation.start();
}

auto player_sim(LevelSim const& simulation) -> player::Sim const& {
    auto const* const result{simulation.player_ship_simulation()};
    EXPECT_NE(result, nullptr);
    return *result;
}

void set_test_turn(player::CommandInterface commands, Vector2d const direction) {
    commands.set_yaw_input(static_cast<float>(direction.x()));
    commands.set_pitch_input(static_cast<float>(direction.y()));
}

auto velocity(LevelSim& simulation) -> Vector3d {
    return player_sim(simulation).physical_state().velocity;
}

}

using namespace player_flight_tests;

TEST(PlayerFlight, QueuedTapProducesOrderedTransitionsOnOneTick) {
    LevelSim simulation{make_data().compile(), *std::pmr::new_delete_resource()};
    start(simulation);
    auto commands{simulation.player_ship_commands().value()};
    commands.start_boost();
    commands.start_boost();
    commands.stop_boost();
    commands.start_fire_laser();
    commands.stop_fire_laser();
    simulation.advance(0.0);
    auto const& events{simulation.read_access().frame_events().player};
    EXPECT_TRUE(events.empty());
    advance_ticks(simulation, 1);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0], player::PlayerEventKind::BoostStarted);
    EXPECT_EQ(events[1], player::PlayerEventKind::BoostStopped);
    EXPECT_EQ(player_sim(simulation).controller_state().effective_action,
              player::BoostBrakeState::None);
    simulation.advance(0.0);
    EXPECT_TRUE(events.empty());
}

TEST(PlayerFlight, FullInputQueueAcceptsNewEdgesAfterTheNextTick) {
    LevelSim simulation{make_data().compile(), *std::pmr::new_delete_resource()};
    start(simulation);
    auto commands{simulation.player_ship_commands().value()};
    for (int index{}; index < 255; ++index) {
        commands.start_boost();
    }
    commands.stop_boost();
    advance_ticks(simulation, 1);
    auto const& events{simulation.read_access().frame_events().player};
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front(), player::PlayerEventKind::BoostStarted);

    commands.stop_boost();
    advance_ticks(simulation, 1);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front(), player::PlayerEventKind::BoostStopped);
}

TEST(PlayerFlight, LethalCommandPublishesAtResolutionAndStoppedAdvanceClearsDeaths) {
    LevelSim simulation{make_data().compile(), *std::pmr::new_delete_resource()};
    auto const& ship{player_sim(simulation)};
    auto const id{ship.unique_entity_id};
    simulation.add_player_health(
        Health{-ship.health(simulation.entity_tables().health).health.value()});
    simulation.add_player_health(Health{10});

    auto const& events{simulation.read_access().frame_events()};
    simulation.advance(simulation.clock().tick_period());
    EXPECT_TRUE(ship.is_alive(simulation.entity_tables().health));
    EXPECT_EQ(events.deaths.num(), 0u);

    start(simulation);
    simulation.advance(0.0);
    EXPECT_TRUE(ship.is_alive(simulation.entity_tables().health));
    advance_ticks(simulation, 1);
    EXPECT_FALSE(ship.is_alive(simulation.entity_tables().health));
    ASSERT_EQ(events.deaths.num(), 1u);
    EXPECT_EQ(events.deaths.victims()[0], id);
    auto const& ledger{simulation.entity_ledger()};
    EXPECT_EQ(ledger.history().death_ticks()[ledger.history_index(id)], events.base_tick);

    simulation.stop_run(RunEndReason::WorldEnd);
    EXPECT_EQ(events.deaths.num(), 1u);
    simulation.advance(0.0);
    EXPECT_EQ(events.deaths.num(), 0u);
}

TEST(PlayerFlight, HeldBoostStartsOnlyWhenBrakeReleases) {
    LevelSim simulation{make_data().compile(), *std::pmr::new_delete_resource()};
    start(simulation);
    auto commands{simulation.player_ship_commands().value()};
    auto const& events{simulation.read_access().frame_events().player};
    commands.start_brake();
    commands.start_boost();
    advance_ticks(simulation, 1);
    EXPECT_TRUE(events.empty());

    commands.stop_brake();
    advance_ticks(simulation, 1);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front(), player::PlayerEventKind::BoostStarted);
    advance_ticks(simulation, 1);
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(player_sim(simulation).controller_state().effective_action,
              player::BoostBrakeState::Boost);
}

TEST(PlayerFlight, StarfoxConvergesToCruiseAndSupportsBoostAndBrake) {
    LevelSim simulation{make_data().compile(), *std::pmr::new_delete_resource()};
    start(simulation);
    auto const& controller{player_sim(simulation).controller_state()};
    advance_ticks(simulation, 600);
    auto const cruise_speed{size(velocity(simulation))};
    EXPECT_NEAR(cruise_speed, 12000.0, 2.0);
    EXPECT_NEAR(velocity(simulation).y(), 0.0, 1.e-6);
    EXPECT_NEAR(velocity(simulation).z(), 0.0, 1.e-6);

    simulation.player_ship_commands()->set_accelerator(1.f);
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
    advance_ticks(simulation, 180);
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::Boost);
    EXPECT_GT(size(velocity(simulation)), cruise_speed);
    EXPECT_LT(player_sim(simulation).energy(), 1.f);

    simulation.player_ship_commands()->set_accelerator(0.f);
    advance_ticks(simulation, 1);
    auto const energy_before_brake{player_sim(simulation).energy()};
    simulation.player_ship_commands()->start_brake();
    advance_ticks(simulation, 180);
    EXPECT_LT(size(velocity(simulation)), cruise_speed);
    EXPECT_LT(player_sim(simulation).energy(), energy_before_brake);
}

TEST(PlayerFlight, StarfoxEmergencyBrakeIsStrongerThanNormalBrake) {
    LevelSim normal{make_data().compile(), *std::pmr::new_delete_resource()};
    LevelSim emergency{make_data().compile(), *std::pmr::new_delete_resource()};
    start(normal);
    start(emergency);
    advance_ticks(normal, 600);
    advance_ticks(emergency, 600);

    normal.player_ship_commands()->start_brake();
    emergency.player_ship_commands()->start_emergency_brake();
    advance_ticks(normal, 30);
    advance_ticks(emergency, 30);

    EXPECT_LT(size(velocity(emergency)), size(velocity(normal)));
}

TEST(PlayerFlight, StarfoxTurnsLevelsRollAndRecoversBoostEnergy) {
    LevelSim simulation{make_data({0.0, 0.0, 30.0}).compile(), *std::pmr::new_delete_resource()};
    start(simulation);
    auto commands{simulation.player_ship_commands().value()};
    auto const& ship{player_sim(simulation)};
    auto const& physical{ship.physical_state()};
    commands.set_pitch_input(0.25f);
    commands.set_yaw_input(0.5f);
    advance_ticks(simulation, 30);
    auto const turn{physical.transform.rotator()};
    EXPECT_NEAR(turn.pitch, -0.986, 0.01);
    EXPECT_NEAR(turn.yaw, 16.742, 0.01);
    EXPECT_NEAR(turn.roll, 29.822, 0.01);
    EXPECT_NEAR(
        dot(physical.velocity, physical.transform.forward()), size(physical.velocity), 0.01);

    commands.set_pitch_input(0.f);
    commands.set_yaw_input(0.f);
    commands.start_boost();
    advance_ticks(simulation, 120);
    EXPECT_NEAR(ship.energy(), 0.5f, 0.001f);
    EXPECT_NEAR(physical.transform.rotator().roll, 14.655, 0.01);
    EXPECT_NEAR(size(physical.velocity), 30000.0, 0.1);

    advance_ticks(simulation, 120);
    EXPECT_NEAR(ship.energy(), 0.f, 0.005f);
    commands.stop_boost();
    advance_ticks(simulation, 420);
    EXPECT_NEAR(ship.energy(), 1.f, 0.001f);
    EXPECT_NEAR(size(physical.velocity), 11941.77, 1.0);
    EXPECT_NEAR(physical.transform.rotator().roll, 0.0, 0.001);
}

}
