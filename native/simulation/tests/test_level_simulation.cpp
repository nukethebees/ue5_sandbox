#include <ioj/sim/testing/level_sim_test_access.h>
#include <ioj/sim/world_aabb_operations.h>
#include <type_traits>
#include <utility>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_lasers()), lasers::Sim const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_capital_ships()),
                             capital_ships::Sim const&>);
static_assert(
    std::is_same_v<decltype(std::declval<LevelSim&>().get_fighters()), fighters::Sim const&>);
static_assert(
    std::is_same_v<decltype(std::declval<LevelSim&>().get_turrets()), turrets::Sim const&>);
static_assert(
    std::is_same_v<decltype(std::declval<LevelSim&>().get_spinners()), spinners::Sim const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_entity_registry()),
                             EntityRegistry const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_mission_manager()),
                             MissionManager const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_spatial_query_manager()),
                             SpatialQueryManager const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_level_telemetry_manager()),
                             LevelTelemetryManager const&>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_player_ship_simulation()),
                             player::Sim const*>);
static_assert(std::is_same_v<decltype(std::declval<LevelSim&>().get_player_ship_commands()),
                             player::CommandInterface*>);

static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<CapitalReadView>().entities.locations.xs[0])>>);
static_assert(
    std::is_const_v<
        std::remove_reference_t<decltype(std::declval<FighterReadView>().entities.teams[0])>>);
static_assert(
    std::is_const_v<std::remove_reference_t<decltype(std::declval<TurretReadView>().changes[0])>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<LaserReadView>().entities.lifetimes_remaining[0])>>);
static_assert(std::is_const_v<std::remove_pointer_t<decltype(LevelReadView::registry)>>);

namespace {
auto make_battle() -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    add_capital_spawn(data, {{-1000.f, 0.f, 0.f}}, Team::Green, -1, 60.f, 60.f, 100);
    add_capital_spawn(data, {{1000.f, 0.f, 0.f}}, Team::White, -1, 60.f, 60.f, 100);
    auto const count{collision::EntityAABBs::num()};
    for (std::int32_t index{}; index < count; ++index) {
        data.entity_bounds.set_half_extents(index, {{10.f, 10.f, 10.f}});
    }
    return data;
}

auto make_overlap_response_battle() -> LevelSimInitData {
    auto data{make_battle()};
    data.overlap_response.damage_per_overlap_detection = 50;
    data.level_events.initial_spawns.capital_spawns.healths = {5000, 5000};
    add_player_spawn(data, {});
    data.player->transform.location = {-1000.0, 0.0, 0.0};
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {150, 150};
    data.entity_bounds.set_half_extents(collision::EntityAABBs::capital_ship_index,
                                        {{100.f, 100.f, 100.f}});
    return data;
}

void add_mission(LevelSimInitData& data) {
    auto& mission{data.level_events.initialisation.mission.emplace()};
    auto const& entities{data.level_events.initial_spawns.capital_spawns.entity_indices};
    mission.mode = levels::LevelMissionMode::KillEnemies;
    mission.kill_count = 1;
    mission.save_results = false;
    mission.hero_entity_indices = {entities[0]};
    mission.required_kill_entity_indices = {entities[1]};
}

void kill_enemy(LevelSim& simulation) {
    DirectDamageEvents events;
    events.add(simulation.get_capital_ships().get_handle(1),
               100,
               simulation.get_capital_ships().get_handle(0));
    LevelSimTestAccess::queue_direct_damage_events(simulation, events.get_const_view());
}
}

TEST(NativeSimulation, LevelSimTelemetryCompletionTest) {
    auto data{make_battle()};
    data.telemetry_metadata.emplace();
    data.telemetry_metadata->run_id = "completion-test";
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt * 1.25);
    simulation.complete_telemetry_run(LevelTelemetryRunEndReason::DurationReached, Team::Green);
    tests::expect_equal(
        simulation.get_state(), OrchestratorState::Paused, "Completed run is paused");
    tests::expect_equal(simulation.get_clock().get_completed_ticks(),
                        std::uint64_t{1},
                        "Only one simulation tick completes");
    auto const record{simulation.take_finalized_telemetry_run()};
    if (tests::expect_true(record.has_value(), "Completion yields a telemetry record")) {
        tests::expect_equal(record->completion.reason,
                            LevelTelemetryRunEndReason::DurationReached,
                            "Completion reason is retained");
        tests::expect_false(record->completion.interrupted,
                            "Explicit completion is not interruption");
        tests::expect_true(record->completion.winning_team == std::optional{Team::Green},
                           "Winning team is retained");
        tests::expect_equal(record->completion.completed_ticks,
                            std::uint64_t{1},
                            "Completion records the current tick");
    }
    tests::expect_false(simulation.take_finalized_telemetry_run().has_value(),
                        "Completion is consumed once");
    simulation.advance(dt * 10.0);
    tests::expect_equal(simulation.get_clock().get_completed_ticks(),
                        std::uint64_t{1},
                        "Completed simulation ignores paused time");
    simulation.start();
    simulation.advance(dt * 2.0);
    tests::expect_equal(simulation.get_clock().get_completed_ticks(),
                        std::uint64_t{3},
                        "Restart continues simulation without accumulating paused time");
    tests::expect_equal(simulation.get_read_view().interpolation_alpha(),
                        0.25,
                        1.e-9,
                        "Completion and restart preserve the accumulated fractional tick");
    return;
}

TEST(NativeSimulation, LevelTelemetryMissionCompletionTest) {
    auto data{make_battle()};
    data.telemetry_metadata = LevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryMissionTest",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    add_mission(data);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt);
    kill_enemy(simulation);
    simulation.advance(dt);

    auto const mission_result{simulation.take_mission_result()};
    tests::expect_equal(simulation.get_state(),
                        OrchestratorState::Running,
                        "Taking the mission result does not pause simulation");
    if (!tests::expect_true(mission_result.has_value(),
                            "Simulation yields the completed mission")) {
        return;
    }

    auto record{simulation.take_finalized_telemetry_run()};
    if (!tests::expect_true(record.has_value(), "Taking the mission result finalizes telemetry")) {
        return;
    }
    tests::expect_equal(record->completion.reason,
                        LevelTelemetryRunEndReason::MissionSucceeded,
                        "Mission success selects the telemetry completion reason");
    tests::expect_false(record->completion.interrupted, "Mission completion is not interrupted");
    if (tests::expect_true(record->completion.mission_state.has_value(),
                           "Mission state is present")) {
        tests::expect_equal(record->completion.mission_state.value(),
                            MissionState::Succeeded,
                            "Mission state is retained");
    }
    return;
}

TEST(NativeSimulation, LaserFrameOutputsTest) {
    auto data{make_battle()};
    data.clock_settings.tick_rate = 10.0;
    add_mission(data);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto queue_shot = [](LevelSim& level, float const location) {
        lasers::SpawnRequests requests;
        requests.add({{location, 0.f, 0.f}},
                     {},
                     {},
                     1,
                     2000.f,
                     10000.f,
                     level.get_capital_ships().get_handle(0),
                     {Team::Green, EntityType::Fighter});
        LevelSimTestAccess::queue_laser_spawns(level, requests.get_const_view());
    };
    queue_shot(simulation, 700.f);
    queue_shot(simulation, 500.f);
    simulation.start();
    simulation.advance(0.425);
    auto const frame{simulation.get_read_view()};
    tests::expect_equal(
        frame.lasers.get_num_instances(), 0, "Impacted lasers leave authoritative storage");
    tests::expect_equal(
        frame.lasers.hits.num(), 2, "Both impacts survive the final empty fixed tick");
    tests::expect_equal(
        frame.lasers.hit_ticks.size(), std::size_t{2}, "Impact tick indices remain aligned");
    if (frame.lasers.hit_ticks.size() == 2) {
        tests::expect_equal(frame.lasers.hit_ticks[0],
                            std::uint64_t{1},
                            "First impact keeps its deterministic tick");
        tests::expect_equal(frame.lasers.hit_ticks[1],
                            std::uint64_t{2},
                            "Second impact keeps its deterministic tick");
        tests::expect_true(frame.lasers.hits.sources[0] ==
                               LaserSource{Team::Green, EntityType::Fighter},
                           "Neutral source is retained after removal");
    }
    simulation.advance(0.0);
    tests::expect_equal(simulation.get_read_view().lasers.hits.num(),
                        0,
                        "Next frame does not repeat consumed impacts");
    return;
}

TEST(NativeSimulation, LevelSimInitialQueriesTest) {
    auto data{make_battle()};
    collision::WorldAABBs static_bounds;
    collision::add(static_bounds, {{-10.f, 490.f, -10.f}}, {{10.f, 510.f, 10.f}});
    LevelSim simulation{std::move(data)};
    simulation.set_static_collision(std::move(static_bounds));
    simulation.finish_initialisation();
    auto const& queries{simulation.get_spatial_query_manager()};
    auto const dynamic_hit{queries.trace_closest({{-1100.f, 0.f, 0.f}}, {{-900.f, 0.f, 0.f}})};
    tests::expect_true(dynamic_hit.hit &&
                           dynamic_hit.entity == simulation.get_capital_ships().get_handle(0),
                       "Initial capital is queryable before the first tick");
    auto const static_hit{queries.trace_closest({{-100.f, 500.f, 0.f}}, {{100.f, 500.f, 0.f}})};
    tests::expect_true(static_hit.hit && static_hit.static_geometry_index == 0,
                       "Initial static collision is queryable before the first tick");
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    tests::expect_true(queries.trace_closest({{-100.f, 500.f, 0.f}}, {{100.f, 500.f, 0.f}}).hit,
                       "Static collision survives the first dynamic rebuild");
    return;
}

TEST(NativeSimulation, LevelSimCompiledInitialisationTest) {
    auto data{make_battle()};
    auto const player_index{add_player_spawn(data, {})};
    auto& capitals_events{data.level_events.initial_spawns.capital_spawns};
    capitals_events.target_entity_indices = {capitals_events.entity_indices[1], player_index};
    add_turret_spawn(data, {{0.f, -1000.f, 0.f}}, {0.f, 90.f, 0.f}, Team::Green, 20, 5);
    add_turret_spawn(data, {{0.f, 1000.f, 0.f}}, {}, Team::White, 30, 7);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const& capitals{simulation.get_capital_ships()};
    tests::expect_true(capitals.get_target_handle(0) == capitals.get_handle(1),
                       "Compiled capital target index maps to its registered handle");
    auto const* player{simulation.get_player_ship_simulation()};
    tests::expect_true(capitals.get_target_handle(1) == player->registry_handle,
                       "Compiled player entity index maps to the player handle");
    tests::expect_equal(simulation.get_entity_registry().get_num_alive_active_entities(),
                        5,
                        "Every compiled initial entity is registered");
    auto const& entities{simulation.get_entity_registry().get_entity_data()};
    auto const entity_count{entities.num()};
    std::int32_t turret_count{};
    for (std::int32_t i{}; i < entity_count; ++i) {
        if (entities.entity_types[i] == EntityType::Turret) {
            auto const rotated{entities.teams[i] == Team::Green};
            tests::expect_equal(
                entities.healths[i], rotated ? 20 : 30, "Compiled turret health is retained");
            tests::expect_equal(entities.rotations.yaws[i],
                                rotated ? 90.f : 0.f,
                                "Compiled turret rotation is retained");
            ++turret_count;
        }
    }
    tests::expect_equal(turret_count, 2, "Both compiled turrets are registered");
    return;
}

TEST(NativeSimulation, LevelSimReconstructionTest) {
    std::optional<LevelSim> simulation;
    tests::expect_false(simulation.has_value(), "Construction can be delayed");
    auto first_data{make_battle()};
    add_mission(first_data);
    simulation.emplace(std::move(first_data));
    simulation->finish_initialisation();
    simulation->start();
    simulation->advance(simulation->get_clock().get_tick_period());
    kill_enemy(*simulation);
    simulation->advance(simulation->get_clock().get_tick_period());
    simulation.reset();
    auto second_data{make_battle()};
    add_mission(second_data);
    simulation.emplace(std::move(second_data));
    simulation->finish_initialisation();
    tests::expect_equal(simulation->get_clock().get_completed_ticks(),
                        std::uint64_t{0},
                        "Fresh clock starts at zero");
    tests::expect_equal(simulation->get_entity_registry().get_num_alive_active_entities(),
                        2,
                        "Fresh registry contains both entities");
    tests::expect_equal(simulation->get_entity_registry().get_num_unique_ids_issued(),
                        2,
                        "Fresh registry has no prior history");
    tests::expect_false(simulation->take_mission_result().has_value(),
                        "No pending result survives reconstruction");
    return;
}

TEST(NativeSimulation, LevelSimPlanarMovementOffsetTest) {
    auto data{make_battle()};
    add_player_spawn(data, {});
    data.player->flight_mode = SpaceShipFlightMode::PlanarVelocity;

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto* const player{simulation.get_player_ship_simulation()};
    if (!tests::expect_not_null(player, "Planar movement fixture has a player")) {
        return;
    }

    auto const dt{simulation.get_clock().get_tick_period()};
    auto local_velocity = [player] {
        return player->get_movement_state().transform.inverse_transform_vector_no_scale(
            player->get_movement_state().velocity);
    };

    simulation.get_player_ship_commands()->set_lateral_move_input(1.f);
    simulation.advance(dt);
    tests::expect_true((std::abs(local_velocity().y - 3000.0) <= 0.1),
                       "Held lateral input adds the configured local offset");
    tests::expect_true((std::abs(player->target_local_planar_velocity_scale.x) <= 1.e-4 &&
                        std::abs(player->target_local_planar_velocity_scale.y) <= 1.e-4),
                       "Lateral input does not change desired planar velocity");

    simulation.get_player_ship_commands()->set_lateral_move_input(0.f);
    simulation.get_player_ship_commands()->set_vertical_move_input(1.f);
    simulation.advance(dt);
    tests::expect_true((std::abs(local_velocity().z - 3000.0) <= 0.1),
                       "Held vertical input adds the configured local offset");
    tests::expect_true((std::abs(local_velocity().y) <= 0.1),
                       "Released lateral input removes its local offset");

    simulation.get_player_ship_commands()->set_vertical_move_input(0.f);
    simulation.advance(dt);
    auto const released_velocity{local_velocity()};
    tests::expect_true((std::abs(released_velocity.z) <= 0.1),
                       "Released vertical input removes its local offset");
    tests::expect_true((std::abs(player->target_local_planar_velocity_scale.x) <= 1.e-4 &&
                        std::abs(player->target_local_planar_velocity_scale.y) <= 1.e-4),
                       "Movement offsets remain temporary");
    return;
}

TEST(NativeSimulation, LevelSimOverlapResponseTest) {
    LevelSim simulation{make_overlap_response_battle()};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    auto* player{simulation.get_player_ship_simulation()};
    if (!tests::expect_not_null(player, "The overlap fixture has a movable low-health entity")) {
        return;
    }

    simulation.get_player_ship_commands()->set_lateral_move_input(1.f);
    auto const player_handle{player->registry_handle};
    auto const capital{simulation.get_capital_ships().get_handle(0)};
    auto const player_id{simulation.get_entity_registry().find_unique_id(player_handle)};
    auto const& registry{simulation.get_entity_registry()};

    for (std::int32_t overlap_detection{}; overlap_detection < 3; ++overlap_detection) {
        simulation.advance(dt);
        auto const events{simulation.get_spatial_query_manager()
                              .get_collision_system()
                              .get_aabb_overlap_events()};
        tests::expect_equal(
            events.entity_entity_overlaps.num(), 1, "The tick captures one unique dynamic overlap");
        tests::expect_equal(
            events.entity_static_overlaps.num(), 0, "The tick captures no static overlap");

        tests::expect_equal(registry.get_health(capital),
                            5000 - (overlap_detection + 1) * 50,
                            "The high-health capital receives damage in the detection tick");
        if (overlap_detection < 2) {
            tests::expect_equal(registry.get_health(player_handle),
                                150 - (overlap_detection + 1) * 50,
                                "The low-health entity receives damage in the detection tick");
        }
    }

    tests::expect_false(registry.is_valid_alive(player_handle),
                        "The low-health entity dies after three detected overlaps");
    tests::expect_equal(registry.get_health(capital),
                        4850,
                        "The capital receives one contribution per detected tick");
    tests::expect_true(registry.get_unique_entities().life_state[player_id.index()] ==
                           LifeState::Unknown,
                       "Overlap death uses the environmental death path");
    tests::expect_equal(
        registry.count_kills(), 0, "Environmental overlap death gives no combat kill");
    return;
}

TEST(NativeSimulation, WorldlessLevelSimulationTest) {
    auto first_data{make_battle()};
    auto second_data{make_battle()};
    add_mission(first_data);
    add_mission(second_data);
    LevelSim first{std::move(first_data)};
    LevelSim second{std::move(second_data)};
    tests::expect_equal(first.get_state(),
                        OrchestratorState::Uninitialised,
                        "Construction leaves external setup open");
    first.advance(1.0);
    tests::expect_equal(first.get_clock().get_completed_ticks(),
                        std::uint64_t{0},
                        "Uninitialised simulation ignores elapsed time");
    first.finish_initialisation();
    second.finish_initialisation();
    tests::expect_equal(first.get_state(),
                        OrchestratorState::Paused,
                        "Finishing initialization pauses the simulation");
    tests::expect_false(first.get_level_telemetry_manager().is_run_recording(),
                        "No run is started without metadata");
    first.pause();
    first.pause();
    tests::expect_true(first.get_player_ship_simulation() == nullptr, "No player is needed");
    tests::expect_equal(first.get_entity_registry().get_num_alive_active_entities(),
                        2,
                        "Both capitals are registered");
    first.start();
    second.start();
    auto const dt{first.get_clock().get_tick_period()};
    first.advance(dt);
    tests::expect_equal(second.get_clock().get_completed_ticks(),
                        std::uint64_t{0},
                        "Independent clock remains at zero");
    kill_enemy(first);
    first.advance(dt);
    second.advance(dt);
    tests::expect_equal(first.get_capital_ships().get_num_instances(),
                        1,
                        "Damage removes only the first battle's enemy");
    tests::expect_equal(
        second.get_capital_ships().get_num_instances(), 2, "Other battle is unaffected");
    auto result{first.take_mission_result()};
    tests::expect_true(result.has_value(), "Worldless mission produces a result");
    if (result.has_value()) {
        tests::expect_equal(result->state, MissionState::Succeeded, "Worldless battle succeeds");
        tests::expect_equal(result->kills, 1, "Worldless battle attributes the kill");
    }
    tests::expect_false(first.take_mission_result().has_value(),
                        "Mission result is delivered once");
    first.pause();
    auto const paused_ticks{first.get_clock().get_completed_ticks()};
    first.advance(dt);
    tests::expect_equal(
        first.get_clock().get_completed_ticks(), paused_ticks, "Paused battle does not advance");
    first.start();
    first.advance(dt);
    tests::expect_equal(
        first.get_clock().get_completed_ticks(), paused_ticks + 1, "Battle resumes");
    return;
}

TEST(NativeSimulation, LevelTelemetryRunRecordTest) {

    auto data{make_battle()};
    data.telemetry_metadata = LevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryTest",
        .level_id = "telemetry-test",
        .level_display_name = "Telemetry Test",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    LevelSim simulation{std::move(data)};
    tests::expect_false(simulation.get_level_telemetry_manager().is_run_recording(),
                        "Construction does not start telemetry recording");
    simulation.finish_initialisation();
    tests::expect_true(simulation.get_level_telemetry_manager().is_run_recording() &&
                           simulation.get_state() == OrchestratorState::Paused,
                       "Finishing starts telemetry while still paused");
    simulation.start();

    auto const& telemetry{simulation.get_level_telemetry_manager()};
    tests::expect_true(telemetry.is_run_recording(),
                       "Simulation initialization starts telemetry recording");

    simulation.advance(1.0);
    simulation.set_time_scale(4.0);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.finalize_telemetry_run(LevelTelemetryRunEndReason::WorldEnd, "test");
    tests::expect_equal(simulation.get_state(),
                        OrchestratorState::Running,
                        "Interrupted telemetry finalization does not pause simulation");

    auto record{simulation.take_finalized_telemetry_run()};
    if (!tests::expect_true(record.has_value(), "Finalized manager yields one run record")) {
        return;
    }
    tests::expect_false(simulation.take_finalized_telemetry_run().has_value(),
                        "A finalized run record is yielded only once");
    tests::expect_equal(record->completion.reason,
                        LevelTelemetryRunEndReason::WorldEnd,
                        "Completion preserves its end reason");
    tests::expect_equal(record->completion.completed_ticks,
                        simulation.get_clock().get_completed_ticks(),
                        "Completion preserves completed ticks");
    auto const& battle{record->battle_samples};
    ASSERT_GE(battle.size(), 3);
    EXPECT_EQ(battle.front().completed_tick, SimTick{0});
    EXPECT_EQ(battle.back().completed_tick, simulation.get_clock().get_completed_ticks());
    EXPECT_DOUBLE_EQ(battle.back().simulated_elapsed_seconds,
                     simulation.get_clock().get_simulation_time());
    EXPECT_DOUBLE_EQ(record->metadata.initial_requested_time_scale, 1.0);

    auto const& series{record->tick_series};
    tests::expect_equal(
        series.active_entities.num(), 1, "An unchanged entity count is only stored once");
}

} // namespace tests
