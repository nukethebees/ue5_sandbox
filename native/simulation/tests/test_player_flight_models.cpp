#include <ioj/sim/level_sim.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <utility>

#include "support/simulation_test_support.h"

namespace ioj::sim::tests {
namespace player_flight_model_tests {
auto slot_for(player::FlightModelPreset const preset) -> player::FlightModelSlot {
    switch (preset) {
        case player::FlightModelPreset::Starfox:
            return player::FlightModelSlot::Up;
        case player::FlightModelPreset::Fighter:
            return player::FlightModelSlot::Right;
        case player::FlightModelPreset::Skater:
            return player::FlightModelSlot::Down;
        case player::FlightModelPreset::Gunship:
            return player::FlightModelSlot::Left;
    }
    return player::FlightModelSlot::Up;
}

auto make_data(player::FlightModelPreset const preset) -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{100000.f, 100000.f, 100000.f}};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    data.clock_settings.tick_rate = 60.0;
    auto const count{collision::EntityAABBs::num()};
    for (std::int32_t index{}; index < count; ++index) {
        data.entity_bounds.set_half_extents(index, {{10.f, 10.f, 10.f}});
    }

    player::PlayerSpawnData spawn{};
    spawn.flight_models.initial_slot = slot_for(preset);
    add_player_spawn(data, spawn);
    return data;
}

void advance_ticks(LevelSim& simulation, std::int32_t const count) {
    auto const dt{simulation.get_clock().get_tick_period()};
    for (std::int32_t tick{}; tick < count; ++tick) {
        simulation.advance(dt);
    }
}

void start(LevelSim& simulation) {
    simulation.finish_initialisation();
    simulation.start();
}

auto player_sim(LevelSim const& simulation) -> player::Sim const& {
    auto const* const result{simulation.get_player_ship_simulation()};
    EXPECT_NE(result, nullptr);
    return *result;
}

auto velocity(LevelSim& simulation) -> ml::Vector3d {
    return player_sim(simulation).get_physical_state().velocity;
}

void expect_velocity_near(ml::Vector3d const& actual,
                          ml::Vector3d const& expected,
                          double const tolerance) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}
}

using namespace player_flight_model_tests;

TEST(NativeSimulationFlightModels, StarfoxConvergesToCruiseAndSupportsBoostAndBrake) {
    LevelSim simulation{make_data(player::FlightModelPreset::Starfox)};
    start(simulation);
    advance_ticks(simulation, 600);
    auto const cruise_speed{velocity(simulation).size()};
    EXPECT_NEAR(cruise_speed, 12000.0, 2.0);

    simulation.get_player_ship_commands()->start_boost();
    advance_ticks(simulation, 180);
    EXPECT_GT(velocity(simulation).size(), cruise_speed);
    EXPECT_LT(player_sim(simulation).get_energy(), 1.f);

    simulation.get_player_ship_commands()->stop_boost();
    simulation.get_player_ship_commands()->start_brake();
    advance_ticks(simulation, 180);
    EXPECT_LT(velocity(simulation).size(), cruise_speed);
}

TEST(NativeSimulationFlightModels, FighterAcceleratesOnlyWithInputAndThenDrags) {
    LevelSim simulation{make_data(player::FlightModelPreset::Fighter)};
    start(simulation);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-6);

    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);
    auto const accelerated{velocity(simulation).size()};
    EXPECT_GT(accelerated, 0.0);

    simulation.get_player_ship_commands()->set_throttle(0.f);
    advance_ticks(simulation, 30);
    EXPECT_LT(velocity(simulation).size(), accelerated);
}

TEST(NativeSimulationFlightModels, SkaterCoastsAndRotationDoesNotRotateVelocity) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    auto const before{velocity(simulation)};

    simulation.get_player_ship_commands()->turn({1.0, 0.0});
    advance_ticks(simulation, 60);
    simulation.get_player_ship_commands()->turn({});

    expect_velocity_near(velocity(simulation), before, 1.e-5);
    EXPECT_GT(player_sim(simulation).get_physical_state().transform.forward().y, 0.8);
}

TEST(NativeSimulationFlightModels, SkaterAddsThrustAlongCurrentFacing) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    auto const original{velocity(simulation)};

    simulation.get_player_ship_commands()->turn({1.0, 0.0});
    advance_ticks(simulation, 90);
    simulation.get_player_ship_commands()->turn({});
    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);

    auto const after{velocity(simulation)};
    EXPECT_NEAR(after.x, original.x, 1.e-3);
    EXPECT_GT(after.y, 0.0);
}

TEST(NativeSimulationFlightModels, SkaterBrakeAndEmergencyBrakeReduceWithoutReversing) {
    LevelSim brake{make_data(player::FlightModelPreset::Skater)};
    LevelSim emergency{make_data(player::FlightModelPreset::Skater)};
    start(brake);
    start(emergency);
    for (auto* simulation : {&brake, &emergency}) {
        simulation->get_player_ship_commands()->set_throttle(1.f);
        advance_ticks(*simulation, 30);
        simulation->get_player_ship_commands()->set_throttle(0.f);
    }

    auto const before{velocity(brake).size()};
    brake.get_player_ship_commands()->start_brake();
    emergency.get_player_ship_commands()->start_emergency_brake();
    advance_ticks(brake, 10);
    advance_ticks(emergency, 10);
    EXPECT_LT(velocity(brake).size(), before);
    EXPECT_GE(velocity(brake).x, 0.0);
    EXPECT_LT(velocity(emergency).size(), velocity(brake).size());
    EXPECT_GE(velocity(emergency).x, 0.0);
}

TEST(NativeSimulationFlightModels, GunshipControlsThreeAxesAndStabilizesNeutralInput) {
    LevelSim simulation{make_data(player::FlightModelPreset::Gunship)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_throttle(1.f);
    commands->set_lateral_move_input(1.f);
    commands->set_vertical_move_input(-1.f);
    advance_ticks(simulation, 30);
    auto const driven{velocity(simulation)};
    EXPECT_GT(driven.x, 0.0);
    EXPECT_GT(driven.y, 0.0);
    EXPECT_LT(driven.z, 0.0);

    commands->set_throttle(0.f);
    commands->set_lateral_move_input(0.f);
    commands->set_vertical_move_input(0.f);
    advance_ticks(simulation, 120);
    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, DirectSlotSelectionPreservesPhysicalStateAtBoundary) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);
    auto const& sim_player{player_sim(simulation)};
    auto const transform{sim_player.get_physical_state().transform};
    auto const before{velocity(simulation)};

    simulation.get_player_ship_commands()->select_flight_model_slot(player::FlightModelSlot::Up);

    EXPECT_EQ(sim_player.get_active_flight_model_slot(), player::FlightModelSlot::Up);
    EXPECT_EQ(sim_player.get_physical_state().transform.location, transform.location);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.x, transform.rotation.x);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.y, transform.rotation.y);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.z, transform.rotation.z);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.w, transform.rotation.w);
    expect_velocity_near(sim_player.get_physical_state().velocity, before, 1.e-8);
    EXPECT_EQ(sim_player.get_controller_state().effective_action, player::BoostBrakeState::None);
}

TEST(NativeSimulationFlightModels, EveryPairwiseSlotTransitionPreservesWorldVelocity) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 20);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    auto const preserved{velocity(simulation)};
    std::array const slots{
        player::FlightModelSlot::Up,
        player::FlightModelSlot::Right,
        player::FlightModelSlot::Down,
        player::FlightModelSlot::Left,
    };

    for (auto const source : slots) {
        simulation.get_player_ship_commands()->select_flight_model_slot(source);
        for (auto const target : slots) {
            simulation.get_player_ship_commands()->select_flight_model_slot(target);
            expect_velocity_near(velocity(simulation), preserved, 1.e-8);
        }
    }
}

TEST(NativeSimulationFlightModels, SwitchingRecomputesActionsFromHeldIntent) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->start_boost();
    ASSERT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Boost);

    commands->select_flight_model_slot(player::FlightModelSlot::Left);

    EXPECT_TRUE(player_sim(simulation).get_flight_intent().boost_held);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Boost);
    commands->stop_boost();
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationFlightModels, SlotReplacementAppliesRuntimeConfigWithoutReconstruction) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto const& sim_player{player_sim(simulation)};
    auto profile{sim_player.get_active_flight_model_profile()};
    profile.customized = true;
    profile.config.translation.forward.normal.positive_acceleration = 1234.f;
    profile.config.maximum_resultant_speed = player::effectively_unlimited_speed;
    EXPECT_TRUE(simulation.get_player_ship_commands()->set_flight_model_slot_profile(
        player::FlightModelSlot::Down, profile));
    EXPECT_TRUE(sim_player.get_active_flight_model_profile().customized);

    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(velocity(simulation).x, 1234.0, 0.1);
}

TEST(NativeSimulationFlightModels, ActionPriorityAndEnergyFallbackAreExplicit) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->start_boost();
    commands->start_brake();
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Brake);
    commands->start_emergency_brake();
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::EmergencyBrake);

    advance_ticks(simulation, 400);
    EXPECT_NEAR(player_sim(simulation).get_energy(), 0.f, 1.e-6f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Brake);
}

TEST(NativeSimulationFlightModels, EffectivelyUnlimitedLimitRemainsFinite) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto const& sim_player{player_sim(simulation)};
    auto profile{sim_player.get_active_flight_model_profile()};
    profile.config.maximum_resultant_speed = player::effectively_unlimited_speed;
    profile.config.boosted_maximum_resultant_speed = player::effectively_unlimited_speed;
    ASSERT_TRUE(simulation.get_player_ship_commands()->set_flight_model_slot_profile(
        player::FlightModelSlot::Down, profile));

    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 600);
    auto const current{velocity(simulation)};
    EXPECT_TRUE(std::isfinite(current.x));
    EXPECT_TRUE(std::isfinite(current.y));
    EXPECT_TRUE(std::isfinite(current.z));
}
} // namespace ioj::sim::tests
