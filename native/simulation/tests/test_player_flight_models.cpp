#include "support/simulation_test_support.h"
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/player/flight_model_evaluator.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include <ioj/testing/math_diff.h>
#include <ioj/testing/structural_diff.h>
#include <ioj/testing/vector_comparison.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace ioj::sim::tests {
namespace player_flight_model_tests {
auto slot_for(player::FlightModelPreset const preset) -> player::FlightModelSlot {
    switch (preset) {
        case player::FlightModelPreset::Starfox:
            return player::FlightModelSlot::First;
        case player::FlightModelPreset::Fighter:
            return player::FlightModelSlot::Second;
        case player::FlightModelPreset::Skater:
            return player::FlightModelSlot::Third;
        case player::FlightModelPreset::Gunship:
            return player::FlightModelSlot::Fourth;
    }
    return player::FlightModelSlot::First;
}

auto make_data(player::FlightModelPreset const preset) -> LevelSimInitData {
    LevelSimInitData data;
    data.collision.grid_geometry = {{16, 16, 4}, {{100000.f, 100000.f, 100000.f}}};
    data.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(16);
    data.tick_memory_capacity_bytes = 1024 * 1024;
    data.phase_memory_capacity_bytes = 1024 * 1024;
    data.capital_ships.fighter_spawn_slots = 0;
    data.clock_settings.tick_rate = ioj::Frequency::make(60.0);
    for (auto const type : EnumTraits<EntityType>::values) {
        data.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }

    player::PlayerSpawnData spawn{};
    spawn.flight_models.initial_slot = slot_for(preset);
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
    simulation.finish_initialisation();
    simulation.start();
}

class SkaterFlightModelTest : public ::testing::Test {
  protected:
    void SetUp() override {
        start(simulation);
        ASSERT_NE(simulation.player_ship_simulation(), nullptr);
    }

    LevelSim simulation{make_data(player::FlightModelPreset::Skater)};
    player::Sim& sim_player{LevelSimTestAccess::player_simulation(simulation)};
    player::FlightModelControllerState const& controller{sim_player.controller_state()};
    std::optional<player::CommandInterface> commands{simulation.player_ship_commands()};
};

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

auto turned_velocity_after_tick(player::FlightModelConfig const& config,
                                Vector3d const initial_velocity = {100.0, 100.0, 0.0}) -> Vector3d {
    player::PlayerSimulationState state;
    state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
    state.physical.velocity = initial_velocity;
    player::seed_flight_model_responses(state, config);
    player::integrate_flight_model(1.f, config, {}, state);
    return state.physical.velocity;
}

auto brake_speed_after_tick(player::ResponseConfig const& response, float const deceleration)
    -> double {
    player::FlightModelConfig config;
    config.brake.target_speed = NonNegativeFinite<float>{0.f};
    config.brake.deceleration = NonNegativeFinite<float>::make(deceleration);
    config.brake.response = response;

    player::PlayerSimulationState state;
    state.physical.velocity = {1000.0, 0.0, 0.0};
    player::seed_flight_model_responses(state, config);
    state.controller.effective_action = player::BoostBrakeState::Brake;
    player::integrate_flight_model(0.25f, config, {}, state);
    return size(state.physical.velocity);
}
}

using namespace player_flight_model_tests;
using ioj::testing::expect_approximately_equal;
using ioj::testing::structurally_equal;

TEST(NativeSimulationFlightModels, StarfoxConvergesToCruiseAndSupportsBoostAndBrake) {
    LevelSim simulation{make_data(player::FlightModelPreset::Starfox)};
    start(simulation);
    auto const& controller{player_sim(simulation).controller_state()};
    simulation.player_ship_commands()->set_right_input(1.f);
    simulation.player_ship_commands()->set_forward_input(-1.f);
    simulation.player_ship_commands()->set_up_input(1.f);
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

TEST(NativeSimulationFlightModels, StarfoxEmergencyBrakeIsStrongerThanNormalBrake) {
    LevelSim normal{make_data(player::FlightModelPreset::Starfox)};
    LevelSim emergency{make_data(player::FlightModelPreset::Starfox)};
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

TEST(NativeSimulationFlightModels, FighterAcceleratesOnlyWithInputAndThenDrags) {
    LevelSim simulation{make_data(player::FlightModelPreset::Fighter)};
    start(simulation);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(size(velocity(simulation)), 0.0, 1.e-6);

    simulation.player_ship_commands()->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    auto const accelerated{size(velocity(simulation))};
    EXPECT_GT(accelerated, 0.0);

    simulation.player_ship_commands()->set_accelerator(0.f);
    advance_ticks(simulation, 30);
    EXPECT_LT(size(velocity(simulation)), accelerated);
}

TEST(NativeSimulationFlightModels, FighterBoostAndBrakeRemainOrthogonal) {
    LevelSim normal{make_data(player::FlightModelPreset::Fighter)};
    LevelSim boosted{make_data(player::FlightModelPreset::Fighter)};
    start(normal);
    start(boosted);
    normal.player_ship_commands()->set_accelerator(1.f);
    boosted.player_ship_commands()->set_accelerator(1.f);
    boosted.player_ship_commands()->start_boost();
    advance_ticks(normal, 30);
    advance_ticks(boosted, 30);
    EXPECT_GT(size(velocity(boosted)), size(velocity(normal)));

    auto const before_brake{size(velocity(boosted))};
    boosted.player_ship_commands()->start_brake();
    advance_ticks(boosted, 10);
    EXPECT_LT(size(velocity(boosted)), before_brake);
    EXPECT_EQ(player_sim(boosted).controller_state().effective_action,
              player::BoostBrakeState::Brake);
}

TEST_F(SkaterFlightModelTest, SkaterCoastsAndRotationDoesNotRotateVelocity) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    auto const before{velocity(simulation)};

    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 60);
    set_test_turn(*commands, {});

    expect_approximately_equal(velocity(simulation), before, 1.e-5);
    EXPECT_GT(sim_player.physical_state().transform.forward().y(), 0.8);
}

TEST_F(SkaterFlightModelTest, SkaterAddsThrustAlongCurrentFacing) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    auto const original{velocity(simulation)};

    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(*commands, {});
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);

    auto const after{velocity(simulation)};
    EXPECT_NEAR(after.x(), original.x(), 1.e-3);
    EXPECT_GT(after.y(), 0.0);
}

TEST_F(SkaterFlightModelTest, SkaterTurningUnderThrustProgressivelyBendsTrajectory) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    auto const before_turn{velocity(simulation)};

    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 30);

    auto const after_turn{velocity(simulation)};
    EXPECT_GT(after_turn.x(), before_turn.x());
    EXPECT_GT(after_turn.y(), 0.0);
    EXPECT_LT(after_turn.y(), after_turn.x());
}

TEST_F(SkaterFlightModelTest, SkaterCanFaceBackwardWhilePreservingForwardTravel) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    auto const preserved{velocity(simulation)};

    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 180);

    expect_approximately_equal(velocity(simulation), preserved, 1.e-5);
    EXPECT_LT(dot(velocity(simulation), sim_player.physical_state().transform.forward()), 0.0);
}

TEST(NativeSimulationFlightModels, SkaterBrakeAndEmergencyBrakeReduceWithoutReversing) {
    LevelSim brake{make_data(player::FlightModelPreset::Skater)};
    LevelSim emergency{make_data(player::FlightModelPreset::Skater)};
    start(brake);
    start(emergency);
    for (auto* simulation : {&brake, &emergency}) {
        simulation->player_ship_commands()->set_accelerator(1.f);
        advance_ticks(*simulation, 30);
        simulation->player_ship_commands()->set_accelerator(0.f);
    }

    auto const before{size(velocity(brake))};
    brake.player_ship_commands()->start_brake();
    emergency.player_ship_commands()->start_emergency_brake();
    advance_ticks(brake, 10);
    advance_ticks(emergency, 10);
    EXPECT_LT(size(velocity(brake)), before);
    EXPECT_GE(velocity(brake).x(), 0.0);
    EXPECT_LT(size(velocity(emergency)), size(velocity(brake)));
    EXPECT_GE(velocity(emergency).x(), 0.0);
}

TEST(NativeSimulationFlightModels, GunshipControlsThreeAxesAndStabilizesNeutralInput) {
    LevelSim simulation{make_data(player::FlightModelPreset::Gunship)};
    start(simulation);
    auto commands{simulation.player_ship_commands()};
    commands->set_forward_input(1.f);
    commands->set_right_input(1.f);
    commands->set_up_input(-1.f);
    advance_ticks(simulation, 30);
    auto const driven{velocity(simulation)};
    EXPECT_GT(driven.x(), 0.0);
    EXPECT_GT(driven.y(), 0.0);
    EXPECT_LT(driven.z(), 0.0);

    commands->set_forward_input(0.f);
    commands->set_right_input(0.f);
    commands->set_up_input(0.f);
    advance_ticks(simulation, 120);
    EXPECT_NEAR(size(velocity(simulation)), 0.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, GunshipRotationRemainsIndependentOfTranslationControl) {
    LevelSim simulation{make_data(player::FlightModelPreset::Gunship)};
    start(simulation);
    auto commands{simulation.player_ship_commands()};
    commands->set_right_input(1.f);
    set_test_turn(*commands, {1.0, 0.5});
    advance_ticks(simulation, 30);

    EXPECT_GT(velocity(simulation).y(), 0.0);
    auto const facing{player_sim(simulation).physical_state().transform.forward()};
    EXPECT_GT(facing.y(), 0.0);
    EXPECT_GT(std::abs(facing.z()), 0.0);
}

TEST_F(SkaterFlightModelTest, DirectSlotSelectionPreservesPhysicalStateAtBoundary) {
    commands->set_accelerator(1.f);
    commands->set_forward_input(-0.25f);
    advance_ticks(simulation, 30);
    set_test_turn(*commands, {0.25, -0.5});
    auto const physical{sim_player.physical_state()};
    auto const resources{sim_player.resource_state()};
    auto expected_presentation{sim_player.presentation_state()};
    ++expected_presentation.boost_start_sequence;
    player::PlayerFlightIntent const expected_intent{
        .translation = {-0.25, 0.0, 0.0}, .rotation = {-0.5, 0.25, 0.0}, .accelerator = 1.f};
    auto const health{sim_player.health()};
    commands->start_fire_laser();
    auto const firing_mode{sim_player.laser_firing_mode};
    auto const fire_rate{sim_player.laser_fire_rate};

    commands->select_flight_model_slot(player::FlightModelSlot::First);

    EXPECT_EQ(sim_player.active_flight_model_slot(), player::FlightModelSlot::First);
    EXPECT_PRED_FORMAT2(structurally_equal, physical, sim_player.physical_state());
    EXPECT_PRED_FORMAT2(structurally_equal, resources, sim_player.resource_state());
    EXPECT_PRED_FORMAT2(structurally_equal, expected_presentation, sim_player.presentation_state());
    EXPECT_PRED_FORMAT2(structurally_equal, expected_intent, sim_player.flight_intent());
    EXPECT_EQ(sim_player.health(), health);
    EXPECT_EQ(sim_player.laser_firing_mode, firing_mode);
    EXPECT_EQ(sim_player.laser_fire_rate, fire_rate);
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::Boost);
}

TEST_F(SkaterFlightModelTest, SwitchingClearsPersistentTargetsAndSamplingState) {
    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{1000.f};
    forward.normal.negative_target_speed = NonNegativeFinite<float>{500.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    sim_player.start_sampling();
    sim_player.set_ship_2d_control({0.5, 0.75});
    sim_player.stop_sampling();
    ASSERT_NE(controller.persistent_forward_target_speed, 0.f);
    sim_player.start_sampling();

    commands->select_flight_model_slot(player::FlightModelSlot::First);

    EXPECT_FALSE(sim_player.is_sampling_target_speed());
    EXPECT_EQ(sim_player.sampled_target_speed_scale(), Vector2d{});
    EXPECT_EQ(controller.persistent_forward_target_speed, 0.f);
    EXPECT_EQ(controller.persistent_right_target_speed, 0.f);
    EXPECT_EQ(controller.persistent_up_target_speed, 0.f);
}

TEST_F(SkaterFlightModelTest, SwitchingRecomputesActionsFromHeldIntent) {
    commands->start_boost();
    ASSERT_EQ(controller.effective_action, player::BoostBrakeState::Boost);

    commands->select_flight_model_slot(player::FlightModelSlot::Fourth);

    player::PlayerFlightIntent const expected_intent{.boost_held = true};
    EXPECT_PRED_FORMAT2(structurally_equal, expected_intent, sim_player.flight_intent());
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::Boost);
    commands->stop_boost();
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
}

TEST_F(SkaterFlightModelTest, SlotReplacementAppliesRuntimeConfigWithoutReconstruction) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.customized = true;
    profile.config.translation.forward.normal.positive_acceleration =
        NonNegativeFinite<float>{1234.f};
    profile.config.maximum_resultant_speed =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    EXPECT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    EXPECT_EQ(sim_player.active_flight_model_profile(), profile);

    commands->set_accelerator(1.f);
    advance_ticks(simulation, 60);
    EXPECT_NEAR(velocity(simulation).x(), 1234.0, 0.1);
}

TEST_F(SkaterFlightModelTest, ResponseSeedingUsesTheNewChannelsReferenceFrame) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(*commands, {});
    auto const world_forward_speed{static_cast<float>(velocity(simulation).x())};
    ASSERT_GT(world_forward_speed, 0.f);
    ASSERT_NEAR(sim_player.physical_state().transform.forward().x(), 0.0, 1.e-3);

    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetVelocity;
    forward.manual.reference_frame = player::ReferenceFrame::World;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    EXPECT_NEAR(controller.forward_manual_response.value(), world_forward_speed, 1.e-3f);
}

TEST(NativeSimulationFlightModels, PassiveDragAndNeutralStabilizationRemainDistinct) {
    LevelSim passive{make_data(player::FlightModelPreset::Skater)};
    LevelSim stabilized{make_data(player::FlightModelPreset::Skater)};
    start(passive);
    start(stabilized);

    auto configure = [](LevelSim& simulation, bool const use_passive_drag) {
        auto commands{simulation.player_ship_commands()};
        auto profile{player_sim(simulation).active_flight_model_profile()};
        auto& forward{profile.config.translation.forward};
        forward.passive_drag = NonNegativeFinite<float>::make(use_passive_drag ? 1000.f : 0.f);
        forward.active_stabilization_rate =
            NonNegativeFinite<float>::make(use_passive_drag ? 0.f : 1000.f);
        EXPECT_TRUE(
            commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
        commands->set_accelerator(1.f);
    };
    configure(passive, true);
    configure(stabilized, false);
    advance_ticks(passive, 30);
    advance_ticks(stabilized, 30);

    EXPECT_LT(velocity(passive).x(), velocity(stabilized).x());
    auto const stabilized_driven_speed{velocity(stabilized).x()};
    stabilized.player_ship_commands()->set_accelerator(0.f);
    advance_ticks(stabilized, 30);
    EXPECT_LT(velocity(stabilized).x(), stabilized_driven_speed);
}

TEST(NativeSimulationFlightModels, PassiveDragUsesItsExplicitReferenceFrame) {
    auto evaluate = [](player::ReferenceFrame const frame) {
        player::FlightModelConfig config;
        config.translation.forward.passive_drag = NonNegativeFinite<float>{40.f};
        config.translation.forward.passive_drag_reference_frame = frame;

        return turned_velocity_after_tick(config);
    };

    expect_approximately_equal(evaluate(player::ReferenceFrame::Ship), {100.0, 60.0, 0.0}, 1.e-5);
    expect_approximately_equal(evaluate(player::ReferenceFrame::World), {60.0, 100.0, 0.0}, 1.e-5);
}

TEST(NativeSimulationFlightModels, ActiveStabilizationUsesItsExplicitReferenceFrame) {
    auto evaluate = [](player::ReferenceFrame const frame) {
        player::FlightModelConfig config;
        config.translation.forward.active_stabilization_rate = NonNegativeFinite<float>{40.f};
        config.translation.forward.active_stabilization_reference_frame = frame;

        return turned_velocity_after_tick(config);
    };

    expect_approximately_equal(evaluate(player::ReferenceFrame::Ship), {100.0, 60.0, 0.0}, 1.e-5);
    expect_approximately_equal(evaluate(player::ReferenceFrame::World), {60.0, 100.0, 0.0}, 1.e-5);
}

TEST(NativeSimulationFlightModels, DisabledManualFrameCannotAffectAxisLevelDamping) {
    auto evaluate = [](player::ReferenceFrame const unused_manual_frame) {
        player::FlightModelConfig config;
        auto& forward{config.translation.forward};
        forward.manual.reference_frame = unused_manual_frame;
        forward.passive_drag = NonNegativeFinite<float>{10.f};
        forward.passive_drag_reference_frame = player::ReferenceFrame::World;
        forward.active_stabilization_rate = NonNegativeFinite<float>{20.f};
        forward.active_stabilization_reference_frame = player::ReferenceFrame::World;

        return turned_velocity_after_tick(config);
    };

    auto const ship_manual{evaluate(player::ReferenceFrame::Ship)};
    auto const world_manual{evaluate(player::ReferenceFrame::World)};
    expect_approximately_equal(ship_manual, {70.0, 100.0, 0.0}, 1.e-5);
    expect_approximately_equal(world_manual, ship_manual, 1.e-5);
}

TEST(NativeSimulationFlightModels, AutomaticAccelerationAndPassiveDragKeepIndependentFrames) {
    auto evaluate = [](player::ReferenceFrame const automatic_frame,
                       player::ReferenceFrame const drag_frame) {
        player::FlightModelConfig config;
        auto& forward{config.translation.forward};
        forward.automatic.semantic = player::TranslationSemantic::Acceleration;
        forward.automatic.reference_frame = automatic_frame;
        forward.automatic.automatic_value = Bounded<float, -1, 1>{1.f};
        forward.normal.positive_acceleration = NonNegativeFinite<float>{10.f};
        forward.passive_drag = NonNegativeFinite<float>{20.f};
        forward.passive_drag_reference_frame = drag_frame;

        return turned_velocity_after_tick(config, {50.0, 50.0, 0.0});
    };

    expect_approximately_equal(
        evaluate(player::ReferenceFrame::World, player::ReferenceFrame::Ship),
        {60.0, 30.0, 0.0},
        1.e-5);
    expect_approximately_equal(
        evaluate(player::ReferenceFrame::Ship, player::ReferenceFrame::World),
        {30.0, 60.0, 0.0},
        1.e-5);
}

TEST_F(SkaterFlightModelTest, BoostResponseChangesWithoutDiscardingAccelerationState) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.boost.response.mode = player::ResponseMode::RateLimited;
    profile.config.boost.response.rate_limited = {NonNegativeFinite<float>{1000.f},
                                                  NonNegativeFinite<float>{1000.f}};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 1);
    auto const before_boost{velocity(simulation).x()};

    commands->start_boost();
    advance_ticks(simulation, 1);
    auto const boost_velocity_change{velocity(simulation).x() - before_boost};

    EXPECT_GT(boost_velocity_change, 100.0);
    EXPECT_LT(boost_velocity_change, 250.0);
}

TEST_F(SkaterFlightModelTest, PartialFacingCouplingBendsVelocityWithoutLockingIt) {
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    set_test_turn(*commands, {1.0, 0.0});
    advance_ticks(simulation, 90);
    set_test_turn(*commands, {});
    ASSERT_GT(velocity(simulation).x(), 0.0);

    auto profile{sim_player.active_flight_model_profile()};
    profile.config.facing_velocity.mode = player::FacingVelocityCoupling::AlignToFacing;
    profile.config.facing_velocity.alignment_rate = NonNegativeFinite<float>{1000.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    advance_ticks(simulation, 30);

    auto const bent{velocity(simulation)};
    EXPECT_GT(bent.x(), 0.0);
    EXPECT_GT(bent.y(), 0.0);
    EXPECT_GT(std::abs(bent.x()), std::abs(bent.y()));
}

TEST(NativeSimulationFlightModels, EffectivelyUnlimitedLimitRemainsFinite) {
    player::FlightModelConfig config;
    config.maximum_resultant_speed =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    config.boosted_maximum_resultant_speed =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    config.translation.forward.manual.semantic = player::TranslationSemantic::Acceleration;
    config.translation.forward.manual.input_source = player::TranslationInputSource::Axis;
    config.translation.forward.normal.positive_acceleration = NonNegativeFinite<float>{120.f};
    player::PlayerSimulationState state;
    state.physical.velocity = {1.e12, -1.e12, 1.e12};
    player::seed_flight_model_responses(state, config);
    player::PlayerFlightIntent intent;
    intent.translation.x() = 1.0;

    player::integrate_flight_model(1.f / 60.f, config, intent, state);
    auto const current{state.physical.velocity};
    EXPECT_TRUE(is_finite(current)) << ::testing::PrintToString(current.elements);
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
    response.rate_limited = {NonNegativeFinite<float>{2.f}, NonNegativeFinite<float>{2.f}};

    EXPECT_GT(brake_speed_after_tick(response, 400.f), brake_speed_after_tick(response, 800.f));
    EXPECT_NEAR(brake_speed_after_tick(response, 400.f), 950.0, 1.e-4);
}

TEST(NativeSimulationFlightModels, SecondOrderBrakeStrengthUsesConfiguredDeceleration) {
    player::ResponseConfig response;
    response.mode = player::ResponseMode::SecondOrder;
    response.second_order = {PositiveFinite<float>{1.f}, OpenInterval<float, 0, 1>{0.5f}};

    auto const normal{brake_speed_after_tick(response, 400.f)};
    auto const heavy{brake_speed_after_tick(response, 800.f)};
    EXPECT_GT(normal, heavy);
    EXPECT_LT(normal, 1000.0);
}

TEST_F(SkaterFlightModelTest, EnergyFallbackRespectsHeldIntentAndAvailability) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.emergency_brake.energy_drain_per_second = NonNegativeFinite<float>{60.f};
    profile.config.brake.available = false;
    profile.config.boost.available = false;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->start_emergency_brake();
    advance_ticks(simulation, 1);

    EXPECT_FLOAT_EQ(sim_player.energy(), 0.f);
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
}

TEST_F(SkaterFlightModelTest, EnergyFallbackUsesOnlyAvailableIndependentlyHeldActions) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.emergency_brake.energy_drain_per_second = NonNegativeFinite<float>{60.f};
    profile.config.brake.energy_drain_per_second = NonNegativeFinite<float>{0.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->start_boost();
    commands->start_brake();
    commands->start_emergency_brake();
    ASSERT_EQ(controller.effective_action, player::BoostBrakeState::EmergencyBrake);
    advance_ticks(simulation, 1);

    EXPECT_FLOAT_EQ(sim_player.energy(), 0.f);
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::Brake);

    commands->stop_brake();
    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
}

TEST_F(SkaterFlightModelTest, UnavailableBoostNeverBecomesEffective) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.boost.available = false;
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->start_boost();

    EXPECT_EQ(controller.effective_action, player::BoostBrakeState::None);
}

TEST_F(SkaterFlightModelTest, AcceleratorCanDriveEveryTranslationAxis) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.translation.forward.manual.semantic = player::TranslationSemantic::Disabled;
    for (auto* const axis :
         std::array{&profile.config.translation.right, &profile.config.translation.up}) {
        axis->manual.semantic = player::TranslationSemantic::Acceleration;
        axis->manual.input_source = player::TranslationInputSource::Accelerator;
        axis->normal.positive_acceleration = NonNegativeFinite<float>{120.f};
    }
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->set_accelerator(1.f);
    advance_ticks(simulation, 60);

    EXPECT_NEAR(velocity(simulation).x(), 0.0, 1.e-6);
    EXPECT_NEAR(velocity(simulation).y(), 120.0, 1.e-3);
    EXPECT_NEAR(velocity(simulation).z(), 120.0, 1.e-3);
}

TEST_F(SkaterFlightModelTest, TargetVelocityUsesInputScaleNotUnlimitedCap) {
    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetVelocity;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{2000.f};
    forward.normal.negative_target_speed = NonNegativeFinite<float>{500.f};
    forward.normal.positive_speed_limit =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    forward.normal.negative_speed_limit =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->set_forward_input(0.5f);
    advance_ticks(simulation, 1);

    EXPECT_NEAR(velocity(simulation).x(), 1000.0, 1.e-5);
    EXPECT_TRUE(std::isfinite(velocity(simulation).x()));
}

TEST_F(SkaterFlightModelTest, TargetSpeedUsesAsymmetricScalesAndTrimBounds) {
    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{1000.f};
    forward.normal.negative_target_speed = NonNegativeFinite<float>{400.f};
    forward.normal.positive_speed_limit =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    forward.normal.negative_speed_limit =
        NonNegativeFinite<float>::make(player::effectively_unlimited_speed);
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    sim_player.start_sampling();
    sim_player.set_ship_2d_control({0.0, -0.5});
    sim_player.stop_sampling();
    EXPECT_FLOAT_EQ(controller.persistent_forward_target_speed, -200.f);
    advance_ticks(simulation, 1);
    EXPECT_NEAR(velocity(simulation).x(), -200.0, 1.e-5);

    for (std::int32_t index{}; index < 100; ++index) {
        sim_player.adjust_desired_forward_velocity(-1.f);
    }
    EXPECT_FLOAT_EQ(controller.persistent_forward_target_speed, -400.f);
    for (std::int32_t index{}; index < 100; ++index) {
        sim_player.adjust_desired_forward_velocity(1.f);
    }
    EXPECT_FLOAT_EQ(controller.persistent_forward_target_speed, 1000.f);
}

TEST_F(SkaterFlightModelTest, ProfileReplacementClampsTargetsAndReseedsResponses) {
    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{1000.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    sim_player.start_sampling();
    sim_player.set_ship_2d_control({0.0, 0.8});
    sim_player.stop_sampling();
    advance_ticks(simulation, 1);
    ASSERT_NEAR(velocity(simulation).x(), 800.0, 1.e-5);
    auto const physical{sim_player.physical_state()};

    forward.normal.positive_target_speed = NonNegativeFinite<float>{300.f};
    forward.normal.positive_speed_limit = NonNegativeFinite<float>{250.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    EXPECT_FLOAT_EQ(controller.persistent_forward_target_speed, 250.f);
    EXPECT_NEAR(controller.forward_manual_response.value(), 800.f, 1.e-5f);
    EXPECT_PRED_FORMAT2(structurally_equal, physical, sim_player.physical_state());
}

TEST_F(SkaterFlightModelTest, LeavingBoostImmediatelyClampsPersistentTarget) {
    auto profile{sim_player.active_flight_model_profile()};
    auto& forward{profile.config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetSpeed;
    forward.manual.input_source = player::TranslationInputSource::Axis;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{100.f};
    forward.boosted.positive_target_speed = NonNegativeFinite<float>{500.f};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));

    commands->start_boost();
    sim_player.start_sampling();
    sim_player.set_ship_2d_control({0.0, 1.0});
    sim_player.stop_sampling();
    ASSERT_FLOAT_EQ(controller.persistent_forward_target_speed, 500.f);

    commands->stop_boost();

    EXPECT_FLOAT_EQ(controller.persistent_forward_target_speed, 100.f);
}

TEST_F(SkaterFlightModelTest, BrakeResponseStateDoesNotSurviveActionTransition) {
    auto profile{sim_player.active_flight_model_profile()};
    profile.config.brake.response.mode = player::ResponseMode::RateLimited;
    profile.config.brake.response.rate_limited = {NonNegativeFinite<float>{1.f},
                                                  NonNegativeFinite<float>{1.f}};
    ASSERT_TRUE(commands->set_flight_model_slot_profile(player::FlightModelSlot::Third, profile));
    commands->set_accelerator(1.f);
    advance_ticks(simulation, 30);
    commands->set_accelerator(0.f);
    commands->start_brake();
    advance_ticks(simulation, 1);
    ASSERT_GT(controller.brake_engagement_response.value(), 0.f);

    commands->stop_brake();

    EXPECT_FLOAT_EQ(controller.brake_engagement_response.value(), 0.f);
}

TEST(NativeSimulationFlightModels, ValidTargetAndAccelerationChannelsComposeAdditively) {
    player::FlightModelConfig config;
    auto& forward{config.translation.forward};
    forward.manual.semantic = player::TranslationSemantic::TargetVelocity;
    forward.normal.positive_target_speed = NonNegativeFinite<float>{1000.f};
    forward.automatic.semantic = player::TranslationSemantic::Acceleration;
    forward.automatic.automatic_value = Bounded<float, -1, 1>{0.5f};
    forward.normal.positive_acceleration = NonNegativeFinite<float>{120.f};

    player::PlayerSimulationState state;
    player::seed_flight_model_responses(state, config);
    player::PlayerFlightIntent intent;
    intent.translation.x() = 0.5;
    player::integrate_flight_model(1.f, config, intent, state);

    EXPECT_NEAR(state.physical.velocity.x(), 560.0, 1.e-5);
}

TEST(NativeSimulationFlightModels, ActiveStabilizationDoesNotFightAutomaticIntent) {
    player::FlightModelConfig config;
    auto& forward{config.translation.forward};
    forward.automatic.semantic = player::TranslationSemantic::Acceleration;
    forward.automatic.automatic_value = Bounded<float, -1, 1>{0.5f};
    forward.normal.positive_acceleration = NonNegativeFinite<float>{120.f};
    forward.active_stabilization_rate = NonNegativeFinite<float>{1000.f};

    player::PlayerSimulationState state;
    player::seed_flight_model_responses(state, config);
    player::integrate_flight_model(1.f, config, {}, state);

    EXPECT_NEAR(state.physical.velocity.x(), 60.0, 1.e-5);
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

    EXPECT_NEAR(state.physical.velocity.x(), 8000.0, 1.e-5);
    EXPECT_GT(state.physical.velocity.y(), 0.0);
    EXPECT_GT(size(state.physical.velocity), 8000.0);
}

TEST(NativeSimulationFlightModels, RotationStabilizationDelayIsIndependentPerAxis) {
    player::FlightModelConfig config;
    config.rotation.yaw.manual_semantic = player::RotationSemantic::TargetAngularVelocity;
    config.rotation.yaw.maximum_rate = NonNegativeFinite<float>{30.f};
    config.rotation.roll.stabilization.enabled = true;
    config.rotation.roll.stabilization.target_angle = Finite<float>{0.f};
    config.rotation.roll.stabilization.delay = NonNegativeFinite<float>{0.5f};

    player::PlayerSimulationState state;
    state.physical.transform.rotation = to_quaternion(Rotator3d{0.0, 0.0, 30.0});
    state.controller.time_since_rotation_input = {1.0, 1.0, 1.0};
    player::seed_flight_model_responses(state, config);
    player::PlayerFlightIntent intent;
    intent.rotation.y() = 1.0;

    player::integrate_flight_model(0.1f, config, intent, state);

    EXPECT_DOUBLE_EQ(state.controller.time_since_rotation_input.y(), 0.0);
    EXPECT_DOUBLE_EQ(state.controller.time_since_rotation_input.z(), 1.0);
    EXPECT_NEAR(state.physical.transform.rotator().roll, 0.0, 1.e-5);
}

TEST(NativeSimulationFlightModels, AuthoredInputTopologyAllowsTuningButRejectsChannelChanges) {
    using namespace player;
    for (auto const preset : {FlightModelPreset::Starfox,
                              FlightModelPreset::Fighter,
                              FlightModelPreset::Skater,
                              FlightModelPreset::Gunship}) {
        SCOPED_TRACE(static_cast<int>(preset));
        auto const authored{make_flight_model_profile(preset).config};
        auto edited{authored};
        auto& speed{edited.translation.forward.normal.positive_target_speed};
        speed = NonNegativeFinite<float>::make(speed + 100.f);
        edited.translation.forward.manual.response.mode = ResponseMode::RateLimited;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));

        edited = authored;
        edited.boost.available = !authored.boost.available;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.boost.accelerator_activates_boost = !authored.boost.accelerator_activates_boost;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.brake.available = !authored.brake.available;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.emergency_brake.available = !authored.emergency_brake.available;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.rotation.roll.stabilization.enabled = !authored.rotation.roll.stabilization.enabled;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.translation.forward.passive_drag_reference_frame =
            authored.translation.forward.passive_drag_reference_frame == ReferenceFrame::Ship
                ? ReferenceFrame::World
                : ReferenceFrame::Ship;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.translation.forward.active_stabilization_reference_frame =
            authored.translation.forward.active_stabilization_reference_frame ==
                    ReferenceFrame::Ship
                ? ReferenceFrame::World
                : ReferenceFrame::Ship;
        EXPECT_TRUE(matches_authored_flight_model_topology(edited, preset));

        edited = authored;
        edited.translation.forward.manual.semantic =
            authored.translation.forward.manual.semantic == TranslationSemantic::Acceleration
                ? TranslationSemantic::TargetVelocity
                : TranslationSemantic::Acceleration;
        EXPECT_FALSE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.translation.forward.manual.input_source =
            authored.translation.forward.manual.input_source == TranslationInputSource::Axis
                ? TranslationInputSource::Accelerator
                : TranslationInputSource::Axis;
        EXPECT_FALSE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.translation.forward.manual.reference_frame =
            authored.translation.forward.manual.reference_frame == ReferenceFrame::Ship
                ? ReferenceFrame::World
                : ReferenceFrame::Ship;
        EXPECT_FALSE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.rotation.pitch.manual_semantic = RotationSemantic::Disabled;
        EXPECT_FALSE(matches_authored_flight_model_topology(edited, preset));
        edited = authored;
        edited.translation.up.manual.semantic =
            authored.translation.up.manual.semantic == TranslationSemantic::Disabled
                ? TranslationSemantic::TargetVelocity
                : TranslationSemantic::Disabled;
        EXPECT_FALSE(matches_authored_flight_model_topology(edited, preset));
    }
}
} // namespace ioj::sim::tests
