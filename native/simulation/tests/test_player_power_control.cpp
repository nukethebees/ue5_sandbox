#include <ioj/sim/level_sim.h>

#include <gtest/gtest.h>

#include <cmath>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {
namespace {
auto make_power_data() -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    data.clock_settings.tick_rate = 60.0;
    auto const count{collision::EntityAABBs::num()};
    for (std::int32_t index{}; index < count; ++index) {
        data.entity_bounds.set_half_extents(index, {{10.f, 10.f, 10.f}});
    }

    player::PlayerSpawnData spawn{};
    spawn.flight_mode = SpaceShipFlightMode::PlanarVelocity;
    spawn.control_mode = SpaceShipControlMode::Power;
    spawn.config.power_max_speed = 1000.f;
    spawn.config.power_acceleration = 1000.f;
    spawn.config.power_boost_max_speed = 2000.f;
    spawn.config.power_boost_acceleration = 2000.f;
    spawn.config.power_brake_deceleration = 1000.f;
    spawn.config.power_emergency_brake_deceleration = 4000.f;
    spawn.config.rotation_speed = 90.f;
    add_player_spawn(data, spawn);
    return data;
}

void advance_ticks(LevelSim& simulation, std::int32_t const count) {
    auto const dt{simulation.get_clock().get_tick_period()};
    for (std::int32_t tick{}; tick < count; ++tick) {
        simulation.advance(dt);
    }
}

void start_power_simulation(LevelSim& simulation) {
    simulation.finish_initialisation();
    simulation.start();
}

auto velocity(LevelSim const& simulation) -> ml::Vector3d {
    auto const* const player{simulation.get_player_ship_simulation()};
    EXPECT_NE(player, nullptr);
    return player != nullptr ? player->get_physical_state().velocity : ml::Vector3d{};
}

void expect_velocity_near(ml::Vector3d const& actual,
                          ml::Vector3d const& expected,
                          double const tolerance) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void accelerate(LevelSim& simulation, float const throttle = 1.f, std::int32_t const ticks = 60) {
    simulation.get_player_ship_commands()->set_throttle(throttle);
    advance_ticks(simulation, ticks);
}
}

TEST(NativeSimulationPowerControl, CoastsWithoutPassiveDrag) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    accelerate(simulation);
    simulation.get_player_ship_commands()->set_throttle(0.f);

    auto const before{velocity(simulation)};
    advance_ticks(simulation, 300);
    auto const after{velocity(simulation)};

    EXPECT_NEAR(after.x, before.x, 1.e-6);
    EXPECT_NEAR(after.y, before.y, 1.e-6);
    EXPECT_NEAR(after.z, before.z, 1.e-6);
}

TEST(NativeSimulationPowerControl, RotationDoesNotRotateCoastingVelocity) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    accelerate(simulation);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    auto const before{velocity(simulation)};

    simulation.get_player_ship_commands()->turn({1.0, 0.0});
    advance_ticks(simulation, 60);
    simulation.get_player_ship_commands()->turn({});

    auto const after{velocity(simulation)};
    auto const forward{
        simulation.get_player_ship_simulation()->get_physical_state().transform.forward()};
    EXPECT_NEAR(after.x, before.x, 1.e-6);
    EXPECT_NEAR(after.y, before.y, 1.e-6);
    EXPECT_GT(forward.y, 0.99);
    EXPECT_LT(std::abs(after.y), 1.e-6);
}

TEST(NativeSimulationPowerControl, ThrustRedirectsVelocityTowardsFacing) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    accelerate(simulation);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    simulation.get_player_ship_commands()->turn({1.0, 0.0});
    advance_ticks(simulation, 60);
    simulation.get_player_ship_commands()->turn({});

    auto const forward{
        simulation.get_player_ship_simulation()->get_physical_state().transform.forward()};
    auto const before{velocity(simulation)};
    auto const before_alignment{ml::dot(before, forward) / before.size()};
    accelerate(simulation, 1.f, 30);
    auto const after{velocity(simulation)};
    auto const after_alignment{ml::dot(after, forward) / after.size()};

    EXPECT_GT(after_alignment, before_alignment);
    EXPECT_LT(after_alignment, 0.999);
}

TEST(NativeSimulationPowerControl, ThrottleIsAnalogAndCapsNormalSpeed) {
    LevelSim partial{make_power_data()};
    LevelSim full{make_power_data()};
    start_power_simulation(partial);
    start_power_simulation(full);
    accelerate(partial, 0.25f, 30);
    accelerate(full, 1.f, 30);

    EXPECT_GT(velocity(full).size(), velocity(partial).size());
    advance_ticks(full, 240);
    EXPECT_LE(velocity(full).size(), 1000.0001);
}

TEST(NativeSimulationPowerControl, BrakeStopsWithoutReversing) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    accelerate(simulation);
    simulation.get_player_ship_commands()->set_throttle(0.f);
    simulation.get_player_ship_commands()->start_brake();
    advance_ticks(simulation, 90);

    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-6);
    advance_ticks(simulation, 90);
    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-6);
}

TEST(NativeSimulationPowerControl, EmergencyBrakeAndBoostUsePowerRates) {
    LevelSim normal{make_power_data()};
    LevelSim emergency{make_power_data()};
    start_power_simulation(normal);
    start_power_simulation(emergency);
    accelerate(normal);
    accelerate(emergency);
    normal.get_player_ship_commands()->set_throttle(0.f);
    emergency.get_player_ship_commands()->set_throttle(0.f);
    normal.get_player_ship_commands()->start_brake();
    emergency.get_player_ship_commands()->start_emergency_brake();
    advance_ticks(normal, 15);
    advance_ticks(emergency, 15);
    EXPECT_GT(velocity(normal).size(), velocity(emergency).size());

    LevelSim boost{make_power_data()};
    start_power_simulation(boost);
    boost.get_player_ship_commands()->start_boost();
    accelerate(boost, 1.f, 45);
    EXPECT_GT(velocity(boost).size(), 1000.0);
    EXPECT_LT(boost.get_player_ship_simulation()->get_energy(), 1.0f);
    EXPECT_EQ(boost.get_player_ship_simulation()->get_controller_state().effective_action,
              player::BoostBrakeState::Boost);
    boost.get_player_ship_commands()->stop_boost();
    advance_ticks(boost, 180);
    EXPECT_LE(velocity(boost).size(), 1000.0001);
    EXPECT_EQ(boost.get_player_ship_simulation()->get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationPowerControl, PowerSelectionUsesPlanarVelocity) {
    auto data{make_power_data()};
    data.player->flight_mode = SpaceShipFlightMode::ForwardSpeed;
    LevelSim simulation{std::move(data)};
    start_power_simulation(simulation);

    auto const* const player{simulation.get_player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->flight_mode, SpaceShipFlightMode::PlanarVelocity);

    simulation.get_player_ship_commands()->set_throttle(1.f);
    advance_ticks(simulation, 30);
    EXPECT_GT(velocity(simulation).size(), 0.0);

    simulation.get_player_ship_commands()->set_flight_mode(SpaceShipFlightMode::ForwardSpeed);
    EXPECT_EQ(player->flight_mode, SpaceShipFlightMode::PlanarVelocity);
}

TEST(NativeSimulationPowerControl, SwitchingNeutralModesPreservesPhysicalVelocity) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    accelerate(simulation);
    auto const power_velocity{velocity(simulation)};

    simulation.get_player_ship_commands()->select_previous_control_mode();
    advance_ticks(simulation, 1);
    expect_velocity_near(velocity(simulation), power_velocity, 1.e-4);
    EXPECT_EQ(simulation.get_player_ship_simulation()->get_controller_state().effective_action,
              player::BoostBrakeState::None);

    simulation.get_player_ship_commands()->start_sampling();
    simulation.get_player_ship_commands()->set_ship_1d_control_y(1.f);
    simulation.get_player_ship_commands()->stop_sampling();
    auto const velocity_mode_start{velocity(simulation).size()};
    advance_ticks(simulation, 30);
    EXPECT_GT(velocity(simulation).size(), velocity_mode_start);
    auto const velocity_mode_velocity{velocity(simulation)};

    simulation.get_player_ship_commands()->select_next_control_mode();
    advance_ticks(simulation, 1);
    EXPECT_EQ(simulation.get_player_ship_simulation()->control_mode, SpaceShipControlMode::Power);
    EXPECT_NEAR(velocity(simulation).size(), velocity_mode_velocity.size(), 1.e-4);
    EXPECT_EQ(simulation.get_player_ship_simulation()->get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationPowerControl, PowerToVelocitySwitchClearsBoostWithoutAddingBoostVelocity) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->start_boost();
    accelerate(simulation, 1.f, 30);
    auto const before{velocity(simulation)};

    commands->select_previous_control_mode();
    auto const& controller{simulation.get_player_ship_simulation()->get_controller_state()};
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
    EXPECT_NEAR(controller.planar_boost_speed, 0.f, 1.e-6f);
    EXPECT_NEAR(simulation.get_player_ship_simulation()->throttle, 0.f, 1.e-6f);
    expect_velocity_near(
        simulation.get_player_ship_simulation()->get_physical_state().velocity, before, 1.e-6);

    advance_ticks(simulation, 1);
    expect_velocity_near(velocity(simulation), before, 1.e-4);
}

TEST(NativeSimulationPowerControl, VelocityToPowerSwitchClearsBoostWithoutInheritingResponse) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->select_previous_control_mode();
    commands->start_boost();
    advance_ticks(simulation, 30);
    auto const before{velocity(simulation)};

    commands->select_next_control_mode();
    auto const& controller{simulation.get_player_ship_simulation()->get_controller_state()};
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
    EXPECT_NEAR(controller.planar_boost_speed, 0.f, 1.e-6f);
    EXPECT_NEAR(simulation.get_player_ship_simulation()->throttle, 0.f, 1.e-6f);
    expect_velocity_near(
        simulation.get_player_ship_simulation()->get_physical_state().velocity, before, 1.e-6);

    advance_ticks(simulation, 1);
    expect_velocity_near(velocity(simulation), before, 1.e-4);
}

TEST(NativeSimulationPowerControl, ModeSwitchClearsPowerBrakeStatesWithoutChangingVelocity) {
    for (auto const state :
         {player::BoostBrakeState::Brake, player::BoostBrakeState::EmergencyBrake}) {
        LevelSim simulation{make_power_data()};
        start_power_simulation(simulation);
        accelerate(simulation);
        auto* const commands{simulation.get_player_ship_commands()};
        if (state == player::BoostBrakeState::Brake) {
            commands->start_brake();
        } else {
            commands->start_emergency_brake();
        }
        auto const before{velocity(simulation)};

        commands->select_previous_control_mode();
        auto const& controller{simulation.get_player_ship_simulation()->get_controller_state()};
        EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
        expect_velocity_near(
            simulation.get_player_ship_simulation()->get_physical_state().velocity, before, 1.e-6);
        advance_ticks(simulation, 1);
        expect_velocity_near(velocity(simulation), before, 1.e-4);
    }

    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->select_previous_control_mode();
    commands->start_brake();
    auto const before{velocity(simulation)};

    commands->select_next_control_mode();
    auto const& controller{simulation.get_player_ship_simulation()->get_controller_state()};
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
    expect_velocity_near(
        simulation.get_player_ship_simulation()->get_physical_state().velocity, before, 1.e-6);
    advance_ticks(simulation, 1);
    expect_velocity_near(velocity(simulation), before, 1.e-4);
}

TEST(NativeSimulationPowerControl, PowerEnergyRulesAreExplicit) {
    LevelSim simulation{make_power_data()};
    start_power_simulation(simulation);
    auto* const commands{simulation.get_player_ship_commands()};

    commands->start_boost();
    advance_ticks(simulation, 30);
    auto const after_boost{simulation.get_player_ship_simulation()->get_energy()};
    EXPECT_LT(after_boost, 1.f);

    commands->stop_boost();
    advance_ticks(simulation, 30);
    auto const after_coasting{simulation.get_player_ship_simulation()->get_energy()};
    EXPECT_GT(after_coasting, after_boost);

    commands->start_brake();
    advance_ticks(simulation, 30);
    auto const after_braking{simulation.get_player_ship_simulation()->get_energy()};
    EXPECT_NEAR(after_braking, after_coasting, 1.e-6f);

    commands->stop_brake();
    commands->start_emergency_brake();
    advance_ticks(simulation, 30);
    auto const after_emergency_braking{simulation.get_player_ship_simulation()->get_energy()};
    EXPECT_LT(after_emergency_braking, after_braking);

    commands->stop_brake();
    advance_ticks(simulation, 30);
    EXPECT_GT(simulation.get_player_ship_simulation()->get_energy(), after_emergency_braking);
}

TEST(NativeSimulationPowerControl, EmergencyBrakeFallsBackToBrakeWhenEnergyDepletes) {
    constexpr float normal_brake_deceleration{50.f};
    auto data{make_power_data()};
    data.player->config.brake_depletion_time = 1.f;
    data.player->config.power_brake_deceleration = normal_brake_deceleration;
    data.player->config.power_emergency_brake_deceleration = 100.f;
    LevelSim simulation{std::move(data)};
    start_power_simulation(simulation);
    accelerate(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_throttle(0.f);
    commands->start_emergency_brake();
    advance_ticks(simulation, 62);

    auto const* const player{simulation.get_player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    EXPECT_NEAR(player->get_energy(), 0.f, 1.e-6f);
    EXPECT_EQ(player->get_controller_state().effective_action, player::BoostBrakeState::Brake);
    auto const before_brake_speed{velocity(simulation).size()};
    EXPECT_GT(before_brake_speed, 0.0);

    advance_ticks(simulation, 30);
    auto const after_brake_speed{velocity(simulation).size()};
    EXPECT_LT(after_brake_speed, before_brake_speed);
    EXPECT_NEAR(after_brake_speed, before_brake_speed - normal_brake_deceleration * 0.5, 1.e-3);
    EXPECT_NEAR(player->get_energy(), 0.f, 1.e-6f);

    commands->stop_brake();
    EXPECT_EQ(player->get_controller_state().effective_action, player::BoostBrakeState::None);
    advance_ticks(simulation, 30);
    EXPECT_GT(player->get_energy(), 0.f);
}

TEST(NativeSimulationPowerControl, EmergencyBrakeWithEmptyEnergyUsesNormalBrake) {
    auto data{make_power_data()};
    data.player->config.boost_depletion_time = 1.f;
    LevelSim simulation{std::move(data)};
    start_power_simulation(simulation);
    accelerate(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_throttle(0.f);
    commands->start_boost();
    advance_ticks(simulation, 61);

    auto const* const player{simulation.get_player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    EXPECT_NEAR(player->get_energy(), 0.f, 1.e-6f);

    commands->start_emergency_brake();
    EXPECT_EQ(player->get_controller_state().effective_action, player::BoostBrakeState::Brake);
    auto const before_brake_speed{velocity(simulation).size()};
    advance_ticks(simulation, 30);
    EXPECT_LT(velocity(simulation).size(), before_brake_speed);
    EXPECT_NEAR(player->get_energy(), 0.f, 1.e-6f);
}
} // namespace ioj::sim::tests
