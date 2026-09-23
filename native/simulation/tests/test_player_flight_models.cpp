#include <ioj/sim/level_sim.h>
#include <ioj/sim/player/flight_model_evaluator.h>

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
    data.grid_geometry = {{16, 16, 4}, {{100000.f, 100000.f, 100000.f}}};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    data.clock_settings.tick_rate = 60.0;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        data.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
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

auto mutable_player_sim(LevelSim& simulation) -> player::Sim& {
    return const_cast<player::Sim&>(player_sim(simulation));
}

void set_test_turn(player::CommandInterface* const commands, ml::Vector2d const direction) {
    commands->set_yaw_input(static_cast<float>(direction.x));
    commands->set_pitch_input(static_cast<float>(direction.y));
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

auto brake_speed_after_tick(player::ResponseConfig const& response, float const deceleration)
    -> double {
    player::FlightModelConfig config;
    config.brake.target_speed = 0.f;
    config.brake.deceleration = deceleration;
    config.brake.response = response;

    player::PlayerSimulationState state;
    state.physical.velocity = {1000.0, 0.0, 0.0};
    player::seed_flight_model_responses(state, config);
    state.controller.effective_action = player::BoostBrakeState::Brake;
    player::integrate_flight_model(0.25f, config, {}, state);
    return state.physical.velocity.size();
}
}

using namespace player_flight_model_tests;

TEST(NativeSimulationFlightModels, StarfoxConvergesToCruiseAndSupportsBoostAndBrake) {
    LevelSim simulation{make_data(player::FlightModelPreset::Starfox)};
    start(simulation);
    simulation.get_player_ship_commands()->set_right_input(1.f);
    simulation.get_player_ship_commands()->set_forward_input(-1.f);
    simulation.get_player_ship_commands()->set_up_input(1.f);
    advance_ticks(simulation, 600);
    auto const cruise_speed{velocity(simulation).size()};
    EXPECT_NEAR(cruise_speed, 12000.0, 2.0);
    EXPECT_NEAR(velocity(simulation).y, 0.0, 1.e-6);
    EXPECT_NEAR(velocity(simulation).z, 0.0, 1.e-6);

    simulation.get_player_ship_commands()->set_accelerator(1.f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::None);
    advance_ticks(simulation, 180);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Boost);
    EXPECT_GT(velocity(simulation).size(), cruise_speed);
    EXPECT_LT(player_sim(simulation).get_energy(), 1.f);

    simulation.get_player_ship_commands()->set_accelerator(0.f);
    advance_ticks(simulation, 1);
    auto const energy_before_brake{player_sim(simulation).get_energy()};
    simulation.get_player_ship_commands()->start_brake();
    advance_ticks(simulation, 180);
    EXPECT_LT(velocity(simulation).size(), cruise_speed);
    EXPECT_LT(player_sim(simulation).get_energy(), energy_before_brake);
}

TEST(NativeSimulationFlightModels, StarfoxEmergencyBrakeIsStrongerThanNormalBrake) {
    LevelSim normal{make_data(player::FlightModelPreset::Starfox)};
    LevelSim emergency{make_data(player::FlightModelPreset::Starfox)};
    start(normal);
    start(emergency);
    advance_ticks(normal, 600);
    advance_ticks(emergency, 600);

    normal.get_player_ship_commands()->start_brake();
    emergency.get_player_ship_commands()->start_emergency_brake();
    advance_ticks(normal, 30);
    advance_ticks(emergency, 30);

    EXPECT_LT(velocity(emergency).size(), velocity(normal).size());
}

TEST(NativeSimulationFlightModels, FighterAcceleratesOnlyWithInputAndThenDrags) {
    LevelSim simulation{make_data(player::FlightModelPreset::Fighter)};
    start(simulation);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-6);

    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    auto const accelerated{velocity(simulation).size()};
    EXPECT_GT(accelerated, 0.0);

    simulation.get_player_ship_commands()->set_accelerator(0.f);
    advance_ticks(simulation, 30);
    EXPECT_LT(velocity(simulation).size(), accelerated);
}

TEST(NativeSimulationFlightModels, FighterBoostAndBrakeRemainOrthogonal) {
    LevelSim normal{make_data(player::FlightModelPreset::Fighter)};
    LevelSim boosted{make_data(player::FlightModelPreset::Fighter)};
    start(normal);
    start(boosted);
    normal.get_player_ship_commands()->set_accelerator(1.f);
    boosted.get_player_ship_commands()->set_accelerator(1.f);
    boosted.get_player_ship_commands()->start_boost();
    advance_ticks(normal, 30);
    advance_ticks(boosted, 30);
    EXPECT_GT(velocity(boosted).size(), velocity(normal).size());

    auto const before_brake{velocity(boosted).size()};
    boosted.get_player_ship_commands()->start_brake();
    advance_ticks(boosted, 10);
    EXPECT_LT(velocity(boosted).size(), before_brake);
    EXPECT_EQ(player_sim(boosted).get_controller_state().effective_action,
              player::BoostBrakeState::Brake);
}

TEST(NativeSimulationFlightModels, SkaterCoastsAndRotationDoesNotRotateVelocity) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    simulation.get_player_ship_commands()->set_accelerator(0.f);
    auto const before{velocity(simulation)};

    set_test_turn(simulation.get_player_ship_commands(), {1.0, 0.0});
    advance_ticks(simulation, 60);
    set_test_turn(simulation.get_player_ship_commands(), {});

    expect_velocity_near(velocity(simulation), before, 1.e-5);
    EXPECT_GT(player_sim(simulation).get_physical_state().transform.forward().y, 0.8);
}

TEST(NativeSimulationFlightModels, SkaterAddsThrustAlongCurrentFacing) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    simulation.get_player_ship_commands()->set_accelerator(0.f);
    auto const original{velocity(simulation)};

    set_test_turn(simulation.get_player_ship_commands(), {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(simulation.get_player_ship_commands(), {});
    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 30);

    auto const after{velocity(simulation)};
    EXPECT_NEAR(after.x, original.x, 1.e-3);
    EXPECT_GT(after.y, 0.0);
}

TEST(NativeSimulationFlightModels, SkaterTurningUnderThrustProgressivelyBendsTrajectory) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    auto const before_turn{velocity(simulation)};

    set_test_turn(commands, {1.0, 0.0});
    advance_ticks(simulation, 30);

    auto const after_turn{velocity(simulation)};
    EXPECT_GT(after_turn.x, before_turn.x);
    EXPECT_GT(after_turn.y, 0.0);
    EXPECT_LT(after_turn.y, after_turn.x);
}

TEST(NativeSimulationFlightModels, SkaterCanFaceBackwardWhilePreservingForwardTravel) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    auto const preserved{velocity(simulation)};

    set_test_turn(commands, {1.0, 0.0});
    advance_ticks(simulation, 180);

    expect_velocity_near(velocity(simulation), preserved, 1.e-5);
    EXPECT_LT(ml::dot(velocity(simulation),
                      player_sim(simulation).get_physical_state().transform.forward()),
              0.0);
}

TEST(NativeSimulationFlightModels, SkaterBrakeAndEmergencyBrakeReduceWithoutReversing) {
    LevelSim brake{make_data(player::FlightModelPreset::Skater)};
    LevelSim emergency{make_data(player::FlightModelPreset::Skater)};
    start(brake);
    start(emergency);
    for (auto* simulation : {&brake, &emergency}) {
        simulation->get_player_ship_commands()->set_accelerator(1.f);
        advance_ticks(*simulation, 30);
        simulation->get_player_ship_commands()->set_accelerator(0.f);
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
    commands->set_forward_input(1.f);
    commands->set_right_input(1.f);
    commands->set_up_input(-1.f);
    advance_ticks(simulation, 30);
    auto const driven{velocity(simulation)};
    EXPECT_GT(driven.x, 0.0);
    EXPECT_GT(driven.y, 0.0);
    EXPECT_LT(driven.z, 0.0);

    commands->set_forward_input(0.f);
    commands->set_right_input(0.f);
    commands->set_up_input(0.f);
    advance_ticks(simulation, 120);
    EXPECT_NEAR(velocity(simulation).size(), 0.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, GunshipRotationRemainsIndependentOfTranslationControl) {
    LevelSim simulation{make_data(player::FlightModelPreset::Gunship)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_right_input(1.f);
    set_test_turn(commands, {1.0, 0.5});
    advance_ticks(simulation, 30);

    EXPECT_GT(velocity(simulation).y, 0.0);
    auto const facing{player_sim(simulation).get_physical_state().transform.forward()};
    EXPECT_GT(facing.y, 0.0);
    EXPECT_GT(std::abs(facing.z), 0.0);
}

TEST(NativeSimulationFlightModels, DirectSlotSelectionPreservesPhysicalStateAtBoundary) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_accelerator(1.f);
    simulation.get_player_ship_commands()->set_forward_input(-0.25f);
    advance_ticks(simulation, 30);
    set_test_turn(simulation.get_player_ship_commands(), {0.25, -0.5});
    auto const& sim_player{player_sim(simulation)};
    auto const transform{sim_player.get_physical_state().transform};
    auto const before{velocity(simulation)};
    auto const energy{sim_player.get_energy()};
    auto const health{sim_player.get_health()};
    auto* const commands{simulation.get_player_ship_commands()};
    commands->start_fire_laser();
    auto const firing_mode{sim_player.laser_firing_mode};
    auto const fire_rate{sim_player.laser_fire_rate};

    commands->select_flight_model_slot(player::FlightModelSlot::Up);

    EXPECT_EQ(sim_player.get_active_flight_model_slot(), player::FlightModelSlot::Up);
    EXPECT_EQ(sim_player.get_physical_state().transform.location, transform.location);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.x, transform.rotation.x);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.y, transform.rotation.y);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.z, transform.rotation.z);
    EXPECT_DOUBLE_EQ(sim_player.get_physical_state().transform.rotation.w, transform.rotation.w);
    expect_velocity_near(sim_player.get_physical_state().velocity, before, 1.e-8);
    EXPECT_FLOAT_EQ(sim_player.get_energy(), energy);
    EXPECT_EQ(sim_player.get_health(), health);
    EXPECT_EQ(sim_player.laser_firing_mode, firing_mode);
    EXPECT_EQ(sim_player.laser_fire_rate, fire_rate);
    EXPECT_DOUBLE_EQ(sim_player.get_flight_intent().translation.x, -0.25);
    EXPECT_FLOAT_EQ(sim_player.get_flight_intent().accelerator, 1.f);
    EXPECT_DOUBLE_EQ(sim_player.get_flight_intent().rotation.x, -0.5);
    EXPECT_DOUBLE_EQ(sim_player.get_flight_intent().rotation.y, 0.25);
    EXPECT_EQ(sim_player.get_controller_state().effective_action, player::BoostBrakeState::Boost);
}

TEST(NativeSimulationFlightModels, SwitchingClearsPersistentTargetsAndSamplingState) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.translation.forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    profile.config.translation.forward.manual.input_source = player::TranslationInputSource::Axis;
    profile.config.translation.forward.normal.positive_target_speed = 1000.f;
    profile.config.translation.forward.normal.negative_target_speed = 500.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
    mutable_player_sim(simulation).start_sampling();
    mutable_player_sim(simulation).set_ship_2d_control({0.5, 0.75});
    mutable_player_sim(simulation).stop_sampling();
    ASSERT_NE(player_sim(simulation).get_controller_state().persistent_forward_target_speed, 0.f);
    mutable_player_sim(simulation).start_sampling();

    commands->select_flight_model_slot(player::FlightModelSlot::Up);

    EXPECT_FALSE(player_sim(simulation).is_sampling_target_speed());
    EXPECT_EQ(player_sim(simulation).get_sampled_target_speed_scale(), ml::Vector2d{});
    EXPECT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed, 0.f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().persistent_right_target_speed, 0.f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().persistent_up_target_speed, 0.f);
}

TEST(NativeSimulationFlightModels, EveryPairwiseSlotTransitionPreservesWorldVelocity) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 20);
    simulation.get_player_ship_commands()->set_accelerator(0.f);
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

    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(velocity(simulation).x, 1234.0, 0.1);
}

TEST(NativeSimulationFlightModels, ResponseSeedingUsesTheNewChannelsReferenceFrame) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    set_test_turn(commands, {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(commands, {});
    auto const world_forward_speed{static_cast<float>(velocity(simulation).x)};
    ASSERT_GT(world_forward_speed, 0.f);
    ASSERT_NEAR(player_sim(simulation).get_physical_state().transform.forward().x, 0.0, 1.e-3);

    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.translation.forward.manual.semantic =
        player::TranslationSemantic::TargetVelocity;
    profile.config.translation.forward.manual.reference_frame = player::ReferenceFrame::World;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    EXPECT_NEAR(player_sim(simulation).get_controller_state().forward_manual_response.value(),
                world_forward_speed,
                1.e-3f);
}

TEST(NativeSimulationFlightModels, PassiveDragAndNeutralStabilizationRemainDistinct) {
    LevelSim passive{make_data(player::FlightModelPreset::Skater)};
    LevelSim stabilized{make_data(player::FlightModelPreset::Skater)};
    start(passive);
    start(stabilized);

    auto configure = [](LevelSim& simulation, bool const use_passive_drag) {
        auto* const commands{simulation.get_player_ship_commands()};
        auto profile{player_sim(simulation).get_active_flight_model_profile()};
        auto& forward{profile.config.translation.forward};
        forward.passive_drag = use_passive_drag ? 1000.f : 0.f;
        forward.active_stabilization_rate = use_passive_drag ? 0.f : 1000.f;
        EXPECT_TRUE(
            commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
        commands->set_accelerator(1.f);
    };
    configure(passive, true);
    configure(stabilized, false);
    advance_ticks(passive, 30);
    advance_ticks(stabilized, 30);

    EXPECT_LT(velocity(passive).x, velocity(stabilized).x);
    auto const stabilized_driven_speed{velocity(stabilized).x};
    stabilized.get_player_ship_commands()->set_accelerator(0.f);
    advance_ticks(stabilized, 30);
    EXPECT_LT(velocity(stabilized).x, stabilized_driven_speed);
}

TEST(NativeSimulationFlightModels, PassiveDragUsesItsExplicitReferenceFrame) {
    auto evaluate = [](player::ReferenceFrame const frame) {
        player::FlightModelConfig config;
        config.translation.forward.passive_drag = 40.f;
        config.translation.forward.passive_drag_reference_frame = frame;

        player::PlayerSimulationState state;
        state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
        state.physical.velocity = {100.0, 100.0, 0.0};
        player::seed_flight_model_responses(state, config);
        player::integrate_flight_model(1.f, config, {}, state);
        return state.physical.velocity;
    };

    expect_velocity_near(evaluate(player::ReferenceFrame::Ship), {100.0, 60.0, 0.0}, 1.e-5);
    expect_velocity_near(evaluate(player::ReferenceFrame::World), {60.0, 100.0, 0.0}, 1.e-5);
}

TEST(NativeSimulationFlightModels, ActiveStabilizationUsesItsExplicitReferenceFrame) {
    auto evaluate = [](player::ReferenceFrame const frame) {
        player::FlightModelConfig config;
        config.translation.forward.active_stabilization_rate = 40.f;
        config.translation.forward.active_stabilization_reference_frame = frame;

        player::PlayerSimulationState state;
        state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
        state.physical.velocity = {100.0, 100.0, 0.0};
        player::seed_flight_model_responses(state, config);
        player::integrate_flight_model(1.f, config, {}, state);
        return state.physical.velocity;
    };

    expect_velocity_near(evaluate(player::ReferenceFrame::Ship), {100.0, 60.0, 0.0}, 1.e-5);
    expect_velocity_near(evaluate(player::ReferenceFrame::World), {60.0, 100.0, 0.0}, 1.e-5);
}

TEST(NativeSimulationFlightModels, DisabledManualFrameCannotAffectAxisLevelDamping) {
    auto evaluate = [](player::ReferenceFrame const unused_manual_frame) {
        player::FlightModelConfig config;
        auto& forward{config.translation.forward};
        forward.manual.reference_frame = unused_manual_frame;
        forward.passive_drag = 10.f;
        forward.passive_drag_reference_frame = player::ReferenceFrame::World;
        forward.active_stabilization_rate = 20.f;
        forward.active_stabilization_reference_frame = player::ReferenceFrame::World;

        player::PlayerSimulationState state;
        state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
        state.physical.velocity = {100.0, 100.0, 0.0};
        player::seed_flight_model_responses(state, config);
        player::integrate_flight_model(1.f, config, {}, state);
        return state.physical.velocity;
    };

    auto const ship_manual{evaluate(player::ReferenceFrame::Ship)};
    auto const world_manual{evaluate(player::ReferenceFrame::World)};
    expect_velocity_near(ship_manual, {70.0, 100.0, 0.0}, 1.e-5);
    expect_velocity_near(world_manual, ship_manual, 1.e-5);
}

TEST(NativeSimulationFlightModels, AutomaticAccelerationAndPassiveDragKeepIndependentFrames) {
    auto evaluate = [](player::ReferenceFrame const automatic_frame,
                       player::ReferenceFrame const drag_frame) {
        player::FlightModelConfig config;
        auto& forward{config.translation.forward};
        forward.automatic.semantic = player::TranslationSemantic::Acceleration;
        forward.automatic.reference_frame = automatic_frame;
        forward.automatic.automatic_value = 1.f;
        forward.normal.positive_acceleration = 10.f;
        forward.passive_drag = 20.f;
        forward.passive_drag_reference_frame = drag_frame;

        player::PlayerSimulationState state;
        state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
        state.physical.velocity = {50.0, 50.0, 0.0};
        player::seed_flight_model_responses(state, config);
        player::integrate_flight_model(1.f, config, {}, state);
        return state.physical.velocity;
    };

    expect_velocity_near(evaluate(player::ReferenceFrame::World, player::ReferenceFrame::Ship),
                         {60.0, 30.0, 0.0},
                         1.e-5);
    expect_velocity_near(evaluate(player::ReferenceFrame::Ship, player::ReferenceFrame::World),
                         {30.0, 60.0, 0.0},
                         1.e-5);
}

TEST(NativeSimulationFlightModels, BoostResponseChangesWithoutDiscardingAccelerationState) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.boost.response.mode = player::ResponseMode::RateLimited;
    profile.config.boost.response.rate_limited = {1000.f, 1000.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 1);
    auto const before_boost{velocity(simulation).x};

    commands->start_boost();
    advance_ticks(simulation, 1);
    auto const boost_velocity_change{velocity(simulation).x - before_boost};

    EXPECT_GT(boost_velocity_change, 100.0);
    EXPECT_LT(boost_velocity_change, 250.0);
}

TEST(NativeSimulationFlightModels, PartialFacingCouplingBendsVelocityWithoutLockingIt) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    set_test_turn(commands, {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(commands, {});
    ASSERT_GT(velocity(simulation).x, 0.0);

    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.facing_velocity.mode = player::FacingVelocityCoupling::AlignToFacing;
    profile.config.facing_velocity.alignment_rate = 1000.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
    advance_ticks(simulation, 30);

    auto const bent{velocity(simulation)};
    EXPECT_GT(bent.x, 0.0);
    EXPECT_GT(bent.y, 0.0);
    EXPECT_GT(std::abs(bent.x), std::abs(bent.y));
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

    simulation.get_player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 600);
    auto const current{velocity(simulation)};
    EXPECT_TRUE(std::isfinite(current.x));
    EXPECT_TRUE(std::isfinite(current.y));
    EXPECT_TRUE(std::isfinite(current.z));
}

TEST(NativeSimulationFlightModels, DirectBrakeStrengthUsesConfiguredDeceleration) {
    player::ResponseConfig response;
    response.mode = player::ResponseMode::Direct;

    EXPECT_GT(brake_speed_after_tick(response, 400.f), brake_speed_after_tick(response, 800.f));
    EXPECT_NEAR(brake_speed_after_tick(response, 400.f), 900.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, RateLimitedBrakeStrengthUsesConfiguredDeceleration) {
    player::ResponseConfig response;
    response.mode = player::ResponseMode::RateLimited;
    response.rate_limited = {2.f, 2.f};

    EXPECT_GT(brake_speed_after_tick(response, 400.f), brake_speed_after_tick(response, 800.f));
    EXPECT_NEAR(brake_speed_after_tick(response, 400.f), 950.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, SecondOrderBrakeStrengthUsesConfiguredDeceleration) {
    player::ResponseConfig response;
    response.mode = player::ResponseMode::SecondOrder;
    response.second_order = {.settling_time = 1.f, .damping_ratio = 0.5f};

    auto const normal{brake_speed_after_tick(response, 400.f)};
    auto const heavy{brake_speed_after_tick(response, 800.f)};
    EXPECT_GT(normal, heavy);
    EXPECT_LT(normal, 1000.0);
}

TEST(NativeSimulationFlightModels, EnergyFallbackRespectsHeldIntentAndAvailability) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.emergency_brake.energy_drain_per_second = 60.f;
    profile.config.brake.available = false;
    profile.config.boost.available = false;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->start_emergency_brake();
    advance_ticks(simulation, 1);

    EXPECT_FLOAT_EQ(player_sim(simulation).get_energy(), 0.f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationFlightModels, EnergyFallbackUsesOnlyAvailableIndependentlyHeldActions) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.emergency_brake.energy_drain_per_second = 60.f;
    profile.config.brake.energy_drain_per_second = 0.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->start_boost();
    commands->start_brake();
    commands->start_emergency_brake();
    ASSERT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::EmergencyBrake);
    advance_ticks(simulation, 1);

    EXPECT_FLOAT_EQ(player_sim(simulation).get_energy(), 0.f);
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::Brake);

    commands->stop_brake();
    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationFlightModels, UnavailableBoostNeverBecomesEffective) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.boost.available = false;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->start_boost();

    EXPECT_EQ(player_sim(simulation).get_controller_state().effective_action,
              player::BoostBrakeState::None);
}

TEST(NativeSimulationFlightModels, AcceleratorCanDriveEveryTranslationAxis) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.translation.forward.manual.semantic = player::TranslationSemantic::Disabled;
    for (auto* const axis :
         std::array{&profile.config.translation.right, &profile.config.translation.up}) {
        axis->manual.semantic = player::TranslationSemantic::Acceleration;
        axis->manual.input_source = player::TranslationInputSource::Accelerator;
        axis->normal.positive_acceleration = 120.f;
    }
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->set_accelerator(1.f);
    advance_ticks(simulation, 60);

    EXPECT_NEAR(velocity(simulation).x, 0.0, 1.e-6);
    EXPECT_NEAR(velocity(simulation).y, 120.0, 1.e-3);
    EXPECT_NEAR(velocity(simulation).z, 120.0, 1.e-3);
}

TEST(NativeSimulationFlightModels, TargetVelocityUsesInputScaleNotUnlimitedCap) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetVelocity;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = 2000.f;
    forward.normal.negative_target_speed = 500.f;
    forward.normal.positive_speed_limit = player::effectively_unlimited_speed;
    forward.normal.negative_speed_limit = player::effectively_unlimited_speed;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->set_forward_input(0.5f);
    advance_ticks(simulation, 1);

    EXPECT_NEAR(velocity(simulation).x, 1000.0, 1.e-5);
    EXPECT_TRUE(std::isfinite(velocity(simulation).x));
}

TEST(NativeSimulationFlightModels, TargetSpeedUsesAsymmetricScalesAndTrimBounds) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = 1000.f;
    forward.normal.negative_target_speed = 400.f;
    forward.normal.positive_speed_limit = player::effectively_unlimited_speed;
    forward.normal.negative_speed_limit = player::effectively_unlimited_speed;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    mutable_player_sim(simulation).start_sampling();
    mutable_player_sim(simulation).set_ship_2d_control({0.0, -0.5});
    mutable_player_sim(simulation).stop_sampling();
    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    -200.f);
    advance_ticks(simulation, 1);
    EXPECT_NEAR(velocity(simulation).x, -200.0, 1.e-5);

    for (std::int32_t index{}; index < 100; ++index) {
        mutable_player_sim(simulation).adjust_desired_forward_velocity(-1.f);
    }
    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    -400.f);
    for (std::int32_t index{}; index < 100; ++index) {
        mutable_player_sim(simulation).adjust_desired_forward_velocity(1.f);
    }
    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    1000.f);
}

TEST(NativeSimulationFlightModels, ProfileReplacementClampsTargetsAndReseedsResponses) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = 1000.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
    mutable_player_sim(simulation).start_sampling();
    mutable_player_sim(simulation).set_ship_2d_control({0.0, 0.8});
    mutable_player_sim(simulation).stop_sampling();
    advance_ticks(simulation, 1);
    ASSERT_NEAR(velocity(simulation).x, 800.0, 1.e-5);

    profile.config.translation.forward.normal.positive_target_speed = 300.f;
    profile.config.translation.forward.normal.positive_speed_limit = 250.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    250.f);
    EXPECT_NEAR(player_sim(simulation).get_controller_state().forward_manual_response.value(),
                800.f,
                1.e-5f);
    EXPECT_NEAR(velocity(simulation).x, 800.0, 1.e-5);
}

TEST(NativeSimulationFlightModels, LeavingBoostImmediatelyClampsPersistentTarget) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = 100.f;
    forward.boosted.positive_target_speed = 500.f;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));

    commands->start_boost();
    mutable_player_sim(simulation).start_sampling();
    mutable_player_sim(simulation).set_ship_2d_control({0.0, 1.0});
    mutable_player_sim(simulation).stop_sampling();
    ASSERT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    500.f);

    commands->stop_boost();

    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().persistent_forward_target_speed,
                    100.f);
}

TEST(NativeSimulationFlightModels, BrakeResponseStateDoesNotSurviveActionTransition) {
    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    start(simulation);
    auto* const commands{simulation.get_player_ship_commands()};
    auto profile{player_sim(simulation).get_active_flight_model_profile()};
    profile.config.brake.response.mode = player::ResponseMode::RateLimited;
    profile.config.brake.response.rate_limited = {1.f, 1.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Down, profile));
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    commands->start_brake();
    advance_ticks(simulation, 1);
    ASSERT_GT(player_sim(simulation).get_controller_state().brake_engagement_response.value(), 0.f);

    commands->stop_brake();

    EXPECT_FLOAT_EQ(player_sim(simulation).get_controller_state().brake_engagement_response.value(),
                    0.f);
}

TEST(NativeSimulationFlightModels, ValidTargetAndAccelerationChannelsComposeAdditively) {
    player::FlightModelConfig config;
    auto& forward{config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetVelocity;
    forward.normal.positive_target_speed = 1000.f;
    forward.automatic.semantic = player::TranslationSemantic::Acceleration;
    forward.automatic.automatic_value = 0.5f;
    forward.normal.positive_acceleration = 120.f;

    player::PlayerSimulationState state;
    player::seed_flight_model_responses(state, config);
    player::PlayerFlightIntent intent;
    intent.translation.x = 0.5;
    player::integrate_flight_model(1.f, config, intent, state);

    EXPECT_NEAR(state.physical.velocity.x, 560.0, 1.e-5);
}

TEST(NativeSimulationFlightModels, ActiveStabilizationDoesNotFightAutomaticIntent) {
    player::FlightModelConfig config;
    auto& forward{config.translation.forward};
    forward.automatic.semantic = player::TranslationSemantic::Acceleration;
    forward.automatic.automatic_value = 0.5f;
    forward.normal.positive_acceleration = 120.f;
    forward.active_stabilization_rate = 1000.f;

    player::PlayerSimulationState state;
    player::seed_flight_model_responses(state, config);
    player::integrate_flight_model(1.f, config, {}, state);

    EXPECT_NEAR(state.physical.velocity.x, 60.0, 1.e-5);
}

TEST(NativeSimulationFlightModels, SkaterUnlimitedResultantCapPreservesExistingComponents) {
    auto const profile{player::make_flight_model_profile(player::FlightModelPreset::Skater)};
    player::PlayerSimulationState state;
    state.physical.velocity = {8000.0, 0.0, 0.0};
    state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
    player::seed_flight_model_responses(state, profile.config);
    player::PlayerFlightIntent intent;
    intent.accelerator = 1.f;

    player::integrate_flight_model(1.f, profile.config, intent, state);

    EXPECT_NEAR(state.physical.velocity.x, 8000.0, 1.e-5);
    EXPECT_GT(state.physical.velocity.y, 0.0);
    EXPECT_GT(state.physical.velocity.size(), 8000.0);
}

TEST(NativeSimulationFlightModels, RotationStabilizationDelayIsIndependentPerAxis) {
    player::FlightModelConfig config;
    config.rotation.yaw.manual_semantic = player::RotationSemantic::TargetAngularVelocity;
    config.rotation.yaw.maximum_rate = 30.f;
    config.rotation.roll.stabilization.enabled = true;
    config.rotation.roll.stabilization.target_angle = 0.f;
    config.rotation.roll.stabilization.delay = 0.5f;

    player::PlayerSimulationState state;
    state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 0.0, 30.0});
    state.controller.time_since_rotation_input = {1.0, 1.0, 1.0};
    player::seed_flight_model_responses(state, config);
    player::PlayerFlightIntent intent;
    intent.rotation.y = 1.0;

    player::integrate_flight_model(0.1f, config, intent, state);

    EXPECT_DOUBLE_EQ(state.controller.time_since_rotation_input.y, 0.0);
    EXPECT_DOUBLE_EQ(state.controller.time_since_rotation_input.z, 1.0);
    EXPECT_NEAR(state.physical.transform.rotator().roll, 0.0, 1.e-5);
}
} // namespace ioj::sim::tests
