#include <sandbox/simulation/world_aabb_operations.h>
#include "support/simulation_test_support.h"
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<FCapitalReadView>().entities.locations.xs[0])>>);
static_assert(
    std::is_const_v<
        std::remove_reference_t<decltype(std::declval<FFighterReadView>().entities.teams[0])>>);
static_assert(
    std::is_const_v<std::remove_reference_t<decltype(std::declval<FTurretReadView>().changes[0])>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<FLaserReadView>().entities.lifetimes_remaining[0])>>);
static_assert(std::is_const_v<std::remove_pointer_t<decltype(FLevelReadView::registry)>>);

namespace {
auto make_battle() -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_spawns.add_defaulted(2);
    data.capital_spawns.teams = {ml::simulation::Team::Green, ml::simulation::Team::White};
    data.capital_spawns.healths = {100, 100};
    data.capital_spawns.initial_spawn_delays = {60.f, 60.f};
    data.capital_spawns.spawn_cooldowns = {60.f, 60.f};
    data.capital_spawns.locations.xs = {-1000.f, 1000.f};
    auto const count{ml::simulation::collision::EntityAABBs::num()};
    for (std::int32_t index{}; index < count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }
    return data;
}

auto make_overlap_response_battle() -> FLevelSimulationInitData {
    auto data{make_battle()};
    data.overlap_response.damage_per_overlap_detection = 50;
    data.capital_spawns.healths = {5000, 5000};
    data.player.emplace();
    data.player->transform.location = {-1000.0, 0.0, 0.0};
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {150, 150};
    data.entity_bounds.half_extent_xs[ml::simulation::collision::EntityAABBs::capital_ship_index] =
        100.f;
    data.entity_bounds.half_extent_ys[ml::simulation::collision::EntityAABBs::capital_ship_index] =
        100.f;
    data.entity_bounds.half_extent_zs[ml::simulation::collision::EntityAABBs::capital_ship_index] =
        100.f;
    return data;
}

void prepare_mission(FLevelSimulation& simulation) {
    auto& mission{simulation.get_mission_manager()};
    mission.set_mission_mode(ml::simulation::MissionMode::KillEnemies);
    mission.set_kill_target(1);
    mission.set_save_mission_results(false);
    mission.add_hero_entity(simulation.get_capital_ships().get_handle(0));
    mission.add_entity_required_to_kill(simulation.get_capital_ships().get_handle(1));
    simulation.finish_initialisation();
}

void kill_enemy(FLevelSimulation& simulation) {
    DirectDamageEvents events;
    events.add(simulation.get_capital_ships().get_handle(1),
               100,
               simulation.get_capital_ships().get_handle(0));
    simulation.get_entity_registry().queue_direct_damage_events(events);
}
}

TEST(NativeSimulation, FLevelSimulationTelemetryCompletionTest) {
    auto data{make_battle()};
    data.telemetry_metadata.emplace();
    data.telemetry_metadata->run_id = "completion-test";
    FLevelSimulation simulation{std::move(data)};
    simulation.finish_initialisation();
    std::int32_t end_tick_calls{};
    simulation.on_end_tick = [&](FLevelSimulation& level) {
        ++end_tick_calls;
        level.complete_telemetry_run(ml::simulation::LevelTelemetryRunEndReason::DurationReached,
                                     ml::simulation::Team::Green);
    };
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt * 3.25);
    ml::simulation_tests::expect_equal(
        end_tick_calls, 1, "Completion stops catch-up after the current tick");
    ml::simulation_tests::expect_equal(
        simulation.get_state(), EOrchestratorState::Paused, "Completed run is paused");
    ml::simulation_tests::expect_equal(simulation.get_clock().get_completed_ticks(),
                                       std::uint64_t{1},
                                       "Only one simulation tick completes");
    auto& telemetry{simulation.get_level_telemetry_manager()};
    auto const record{telemetry.take_finalized_run()};
    if (ml::simulation_tests::expect_true(record.has_value(),
                                          "Completion yields a telemetry record")) {
        ml::simulation_tests::expect_equal(
            record->completion.reason,
            ml::simulation::LevelTelemetryRunEndReason::DurationReached,
            "Completion reason is retained");
        ml::simulation_tests::expect_false(record->completion.interrupted,
                                           "Explicit completion is not interruption");
        ml::simulation_tests::expect_true(record->completion.winning_team ==
                                              std::optional{ml::simulation::Team::Green},
                                          "Winning team is retained");
        ml::simulation_tests::expect_equal(record->completion.completed_ticks,
                                           std::uint64_t{1},
                                           "Completion records the current tick");
    }
    ml::simulation_tests::expect_false(telemetry.take_finalized_run().has_value(),
                                       "Completion is consumed once");
    simulation.advance(dt * 10.0);
    ml::simulation_tests::expect_equal(simulation.get_clock().get_completed_ticks(),
                                       std::uint64_t{1},
                                       "Completed simulation ignores paused time");
    simulation.on_end_tick = {};
    simulation.start();
    simulation.advance(0.0);
    ml::simulation_tests::expect_equal(
        simulation.get_clock().get_completed_ticks(),
        std::uint64_t{3},
        "Completion and restart preserve accumulated simulation time");
    return;
}

TEST(NativeSimulation, FLevelTelemetryMissionCompletionTest) {
    auto data{make_battle()};
    data.telemetry_metadata = FLevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryMissionTest",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    FLevelSimulation simulation{std::move(data)};
    prepare_mission(simulation);
    simulation.start();

    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt);
    kill_enemy(simulation);
    simulation.advance(dt);

    auto const mission_result{simulation.take_mission_result()};
    ml::simulation_tests::expect_equal(simulation.get_state(),
                                       EOrchestratorState::Running,
                                       "Taking the mission result does not pause simulation");
    if (!ml::simulation_tests::expect_true(mission_result.has_value(),
                                           "Simulation yields the completed mission")) {
        return;
    }

    auto record{simulation.get_level_telemetry_manager().take_finalized_run()};
    if (!ml::simulation_tests::expect_true(record.has_value(),
                                           "Taking the mission result finalizes telemetry")) {
        return;
    }
    ml::simulation_tests::expect_equal(record->completion.reason,
                                       ml::simulation::LevelTelemetryRunEndReason::MissionSucceeded,
                                       "Mission success selects the telemetry completion reason");
    ml::simulation_tests::expect_false(record->completion.interrupted,
                                       "Mission completion is not interrupted");
    if (ml::simulation_tests::expect_true(record->completion.mission_state.has_value(),
                                          "Mission state is present")) {
        ml::simulation_tests::expect_equal(record->completion.mission_state.value(),
                                           ml::simulation::MissionState::Succeeded,
                                           "Mission state is retained");
    }
    return;
}

TEST(NativeSimulation, FLaserFrameOutputsTest) {
    auto data{make_battle()};
    data.clock_settings.tick_rate = 10.0;
    FLevelSimulation simulation{std::move(data)};
    prepare_mission(simulation);
    auto queue_shot = [](FLevelSimulation& level) {
        ml::simulation::lasers::SpawnRequests requests;
        requests.add_uninitialised(1);
        requests.locations.set(0, ml::simulation::Vector3f{{700.f, 0.f, 0.f}});
        requests.rotations.set(0, ml::simulation::Rotator3f{});
        requests.base_velocities.set(0, ml::simulation::Vector3f{});
        requests.damages[0] = 1;
        requests.speeds[0] = 2000.f;
        requests.max_distances[0] = 10000.f;
        requests.instigator_handles[0] = level.get_capital_ships().get_handle(0);
        requests.sources[0] = ml::simulation::LaserSource{
            ml::simulation::Team::Green, ml::simulation::EntityType::CapitalShipFighter};
        level.get_lasers().queue_laser_spawns(requests.get_const_view());
    };
    queue_shot(simulation);
    simulation.on_end_tick = [&](FLevelSimulation& level) {
        if (level.get_clock().get_completed_ticks() == 1) {
            queue_shot(level);
        }
    };
    simulation.start();
    simulation.advance(0.425);
    auto const frame{simulation.get_read_view()};
    ml::simulation_tests::expect_equal(
        frame.lasers.get_num_instances(), 0, "Impacted lasers leave authoritative storage");
    ml::simulation_tests::expect_equal(
        frame.lasers.hits.num(), 2, "Both impacts survive the final empty fixed tick");
    ml::simulation_tests::expect_equal(
        frame.lasers.hit_ticks.size(), std::size_t{2}, "Impact tick indices remain aligned");
    if (frame.lasers.hit_ticks.size() == 2) {
        ml::simulation_tests::expect_equal(frame.lasers.hit_ticks[0],
                                           std::uint64_t{1},
                                           "First impact keeps its deterministic tick");
        ml::simulation_tests::expect_equal(frame.lasers.hit_ticks[1],
                                           std::uint64_t{2},
                                           "Second impact keeps its deterministic tick");
        ml::simulation_tests::expect_true(
            frame.lasers.hits.sources[0] ==
                ml::simulation::LaserSource{ml::simulation::Team::Green,
                                            ml::simulation::EntityType::CapitalShipFighter},
            "Neutral source is retained after removal");
    }
    simulation.advance(0.0);
    ml::simulation_tests::expect_equal(simulation.get_read_view().lasers.hits.num(),
                                       0,
                                       "Next frame does not repeat consumed impacts");
    return;
}

TEST(NativeSimulation, FLevelSimulationInitialQueriesTest) {
    auto data{make_battle()};
    ml::simulation::collision::add(
        data.static_bounds, {{-10.f, 490.f, -10.f}}, {{10.f, 510.f, 10.f}});
    FLevelSimulation simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const& queries{simulation.get_spatial_query_manager()};
    ml::simulation_tests::expect_equal(
        queries.get_runtime_telemetry().grid_rebuild_count,
        std::uint64_t{0},
        "Initialization rebuilds are excluded from runtime counters");
    auto const dynamic_hit{queries.trace_closest({{-1100.f, 0.f, 0.f}}, {{-900.f, 0.f, 0.f}})};
    ml::simulation_tests::expect_true(
        dynamic_hit.hit && dynamic_hit.entity == simulation.get_capital_ships().get_handle(0),
        "Initial capital is queryable before the first tick");
    auto const static_hit{queries.trace_closest({{-100.f, 500.f, 0.f}}, {{100.f, 500.f, 0.f}})};
    ml::simulation_tests::expect_true(
        static_hit.hit && static_hit.static_geometry_index == 0,
        "Initial static collision is queryable before the first tick");
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    ml::simulation_tests::expect_true(
        queries.trace_closest({{-100.f, 500.f, 0.f}}, {{100.f, 500.f, 0.f}}).hit,
        "Static collision survives the first dynamic rebuild");
    return;
}

TEST(NativeSimulation, FLevelSimulationAuthoredInitialisationTest) {
    for (auto const with_mission : {false, true}) {
        auto data{make_battle()};
        if (with_mission) {
            data.level_events.initialisation.mission.emplace();
        } else {
            data.level_events.initialisation.entity_count = 1;
        }
        FLevelSimulation simulation{std::move(data)};
        simulation.finish_initialisation();
        ml::simulation_tests::expect_equal(
            simulation.get_capital_ships().get_num_instances(),
            0,
            "Authored mission or entity count prevents legacy capital spawns");
    }
    return;
}

TEST(NativeSimulation, FLevelSimulationLegacyInitialisationTest) {
    for (auto const with_player : {false, true}) {
        auto data{make_battle()};
        if (with_player) {
            data.player.emplace();
        }
        data.capital_target_spawn_indices = {1,
                                             FLevelSimulationInitData::player_target_spawn_index};
        data.turret_spawns.add_defaulted(2);
        data.turret_spawns.teams = {ml::simulation::Team::Green, ml::simulation::Team::White};
        data.turret_spawns.healths = {20, 30};
        data.turret_spawns.laser_damages = {5, 7};
        data.turret_spawns.locations.ys = {-1000.f, 1000.f};
        data.turret_transforms.push_back(ml::simulation::Transform3d{
            .rotation = ml::simulation::to_quaternion(ml::simulation::Rotator3d{0.0, 90.0, 0.0})});
        FLevelSimulation simulation{std::move(data)};
        simulation.finish_initialisation();
        auto const& capitals{simulation.get_capital_ships()};
        ml::simulation_tests::expect_true(capitals.get_target_handle(0) == capitals.get_handle(1),
                                          "Capital spawn indices map to registered handles");
        auto const* player{simulation.get_player_ship_simulation()};
        ml::simulation_tests::expect_true(
            capitals.get_target_handle(1) ==
                (player ? player->registry_handle : FRegistryEntityHandle{}),
            "Player target sentinel respects optional player offset");
        ml::simulation_tests::expect_equal(
            simulation.get_entity_registry().get_num_alive_active_entities(),
            with_player ? 5 : 4,
            "All legacy entities are registered");
        auto const& entities{simulation.get_entity_registry().get_entity_data()};
        auto const entity_count{entities.num()};
        std::int32_t turret_count{};
        for (std::int32_t i{}; i < entity_count; ++i) {
            if (entities.entity_types[i] == ml::simulation::EntityType::Turret) {
                auto const explicit_rotation{entities.teams[i] == ml::simulation::Team::Green};
                ml::simulation_tests::expect_equal(entities.healths[i],
                                                   explicit_rotation ? 20 : 30,
                                                   "Legacy turret health survives conversion");
                ml::simulation_tests::expect_equal(
                    entities.rotations.yaws[i],
                    explicit_rotation ? 90.f : 0.f,
                    "Turret rotation uses the transform or defaults to zero");
                ++turret_count;
            }
        }
        ml::simulation_tests::expect_equal(turret_count, 2, "Both legacy turrets are registered");
    }
    return;
}

TEST(NativeSimulation, FLevelSimulationReconstructionTest) {
    std::optional<FLevelSimulation> simulation;
    ml::simulation_tests::expect_false(simulation.has_value(), "Construction can be delayed");
    simulation.emplace(make_battle());
    prepare_mission(*simulation);
    simulation->start();
    simulation->advance(simulation->get_clock().get_tick_period());
    kill_enemy(*simulation);
    simulation->advance(simulation->get_clock().get_tick_period());
    simulation.reset();
    simulation.emplace(make_battle());
    prepare_mission(*simulation);
    ml::simulation_tests::expect_equal(simulation->get_clock().get_completed_ticks(),
                                       std::uint64_t{0},
                                       "Fresh clock starts at zero");
    ml::simulation_tests::expect_equal(
        simulation->get_entity_registry().get_num_alive_active_entities(),
        2,
        "Fresh registry contains both entities");
    ml::simulation_tests::expect_equal(
        simulation->get_entity_registry().get_num_unique_ids_issued(),
        2,
        "Fresh registry has no prior history");
    ml::simulation_tests::expect_false(simulation->get_mission_manager().take_result().has_value(),
                                       "No pending result survives reconstruction");
    return;
}

TEST(NativeSimulation, FLevelSimulationPlanarMovementOffsetTest) {
    auto data{make_battle()};
    data.player.emplace();
    data.player->flight_mode = ml::simulation::SpaceShipFlightMode::PlanarVelocity;

    FLevelSimulation simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto* const player{simulation.get_player_ship_simulation()};
    if (!ml::simulation_tests::expect_not_null(player, "Planar movement fixture has a player")) {
        return;
    }

    auto const dt{simulation.get_clock().get_tick_period()};
    auto local_velocity = [player] {
        return player->transform.inverse_transform_vector_no_scale(player->velocity);
    };

    player->set_lateral_move_input(1.f);
    simulation.advance(dt);
    ml::simulation_tests::expect_true((std::abs(local_velocity().y - 3000.0) <= 0.1),
                                      "Held lateral input adds the configured local offset");
    ml::simulation_tests::expect_true(
        (std::abs(player->target_local_planar_velocity_scale.x) <= 1.e-4 &&
         std::abs(player->target_local_planar_velocity_scale.y) <= 1.e-4),
        "Lateral input does not change desired planar velocity");

    player->set_lateral_move_input(0.f);
    player->set_vertical_move_input(1.f);
    simulation.advance(dt);
    ml::simulation_tests::expect_true((std::abs(local_velocity().z - 3000.0) <= 0.1),
                                      "Held vertical input adds the configured local offset");
    ml::simulation_tests::expect_true((std::abs(local_velocity().y) <= 0.1),
                                      "Released lateral input removes its local offset");

    player->set_vertical_move_input(0.f);
    simulation.advance(dt);
    auto const released_velocity{local_velocity()};
    ml::simulation_tests::expect_true((std::abs(released_velocity.z) <= 0.1),
                                      "Released vertical input removes its local offset");
    ml::simulation_tests::expect_true(
        (std::abs(player->target_local_planar_velocity_scale.x) <= 1.e-4 &&
         std::abs(player->target_local_planar_velocity_scale.y) <= 1.e-4),
        "Movement offsets remain temporary");
    return;
}

TEST(NativeSimulation, FLevelSimulationOverlapResponseTest) {
    FLevelSimulation simulation{make_overlap_response_battle()};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    auto* player{simulation.get_player_ship_simulation()};
    if (!ml::simulation_tests::expect_not_null(
            player, "The overlap fixture has a movable low-health entity")) {
        return;
    }

    player->set_lateral_move_input(1.f);
    auto const player_handle{player->registry_handle};
    auto const capital{simulation.get_capital_ships().get_handle(0)};
    auto const player_id{simulation.get_entity_registry().find_unique_id(player_handle)};
    auto& registry{simulation.get_entity_registry()};

    for (std::int32_t overlap_detection{}; overlap_detection < 3; ++overlap_detection) {
        simulation.advance(dt);
        auto const events{simulation.get_spatial_query_manager()
                              .get_collision_system()
                              .get_aabb_overlap_events()};
        ml::simulation_tests::expect_equal(
            events.entity_entity_overlaps.num(), 1, "The tick captures one unique dynamic overlap");
        ml::simulation_tests::expect_equal(
            events.entity_static_overlaps.num(), 0, "The tick captures no static overlap");

        ml::simulation_tests::expect_equal(
            registry.get_health(capital),
            5000 - (overlap_detection + 1) * 50,
            "The high-health capital receives damage in the detection tick");
        if (overlap_detection < 2) {
            ml::simulation_tests::expect_equal(
                registry.get_health(player_handle),
                150 - (overlap_detection + 1) * 50,
                "The low-health entity receives damage in the detection tick");
        }
    }

    ml::simulation_tests::expect_false(registry.is_valid_alive(player_handle),
                                       "The low-health entity dies after three detected overlaps");
    ml::simulation_tests::expect_equal(registry.get_health(capital),
                                       4850,
                                       "The capital receives one contribution per detected tick");
    ml::simulation_tests::expect_true(registry.get_unique_entities().death_reason[player_id.id] ==
                                          ml::simulation::DeathReason::Unknown,
                                      "Overlap death uses the environmental death path");
    ml::simulation_tests::expect_equal(
        registry.count_kills(), 0, "Environmental overlap death gives no combat kill");
    return;
}

TEST(NativeSimulation, FWorldlessLevelSimulationTest) {
    FLevelSimulation first{make_battle()};
    FLevelSimulation second{make_battle()};
    ml::simulation_tests::expect_equal(first.get_state(),
                                       EOrchestratorState::Uninitialised,
                                       "Construction leaves external setup open");
    first.advance(1.0);
    ml::simulation_tests::expect_equal(first.get_clock().get_completed_ticks(),
                                       std::uint64_t{0},
                                       "Uninitialised simulation ignores elapsed time");
    prepare_mission(first);
    prepare_mission(second);
    ml::simulation_tests::expect_equal(first.get_state(),
                                       EOrchestratorState::Paused,
                                       "Finishing initialization pauses the simulation");
    ml::simulation_tests::expect_false(first.get_level_telemetry_manager().is_run_recording(),
                                       "No run is started without metadata");
    first.pause();
    first.pause();
    ml::simulation_tests::expect_true(first.get_player_ship_simulation() == nullptr,
                                      "No player is needed");
    ml::simulation_tests::expect_equal(first.get_entity_registry().get_num_alive_active_entities(),
                                       2,
                                       "Both capitals are registered");
    first.start();
    second.start();
    auto const dt{first.get_clock().get_tick_period()};
    first.advance(dt);
    ml::simulation_tests::expect_equal(second.get_clock().get_completed_ticks(),
                                       std::uint64_t{0},
                                       "Independent clock remains at zero");
    kill_enemy(first);
    first.advance(dt);
    second.advance(dt);
    ml::simulation_tests::expect_equal(first.get_capital_ships().get_num_instances(),
                                       1,
                                       "Damage removes only the first battle's enemy");
    ml::simulation_tests::expect_equal(
        second.get_capital_ships().get_num_instances(), 2, "Other battle is unaffected");
    auto result{first.get_mission_manager().take_result()};
    ml::simulation_tests::expect_true(result.has_value(), "Worldless mission produces a result");
    if (result.has_value()) {
        ml::simulation_tests::expect_equal(
            result->state, ml::simulation::MissionState::Succeeded, "Worldless battle succeeds");
        ml::simulation_tests::expect_equal(
            result->kills, 1, "Worldless battle attributes the kill");
    }
    ml::simulation_tests::expect_false(first.get_mission_manager().take_result().has_value(),
                                       "Mission result is delivered once");
    first.pause();
    auto const paused_ticks{first.get_clock().get_completed_ticks()};
    first.advance(dt);
    ml::simulation_tests::expect_equal(
        first.get_clock().get_completed_ticks(), paused_ticks, "Paused battle does not advance");
    first.start();
    first.advance(dt);
    ml::simulation_tests::expect_equal(
        first.get_clock().get_completed_ticks(), paused_ticks + 1, "Battle resumes");
    return;
}

TEST(NativeSimulation, FLevelTelemetryRunRecordTest) {

    auto data{make_battle()};
    data.telemetry_metadata = FLevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryTest",
        .level_id = "telemetry-test",
        .level_display_name = "Telemetry Test",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    FLevelSimulation simulation{std::move(data)};
    ml::simulation_tests::expect_false(simulation.get_level_telemetry_manager().is_run_recording(),
                                       "Construction does not start telemetry recording");
    simulation.finish_initialisation();
    ml::simulation_tests::expect_true(simulation.get_level_telemetry_manager().is_run_recording() &&
                                          simulation.get_state() == EOrchestratorState::Paused,
                                      "Finishing starts telemetry while still paused");
    simulation.start();

    auto& telemetry{simulation.get_level_telemetry_manager()};
    ml::simulation_tests::expect_true(telemetry.is_run_recording(),
                                      "Simulation initialization starts telemetry recording");

    simulation.advance(1.0);
    auto const time_scale_change_tick{simulation.get_clock().get_completed_ticks() + 1};
    simulation.set_time_scale(4.0);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.finalize_telemetry_run(ml::simulation::LevelTelemetryRunEndReason::WorldEnd, "test");
    ml::simulation_tests::expect_equal(
        simulation.get_state(),
        EOrchestratorState::Running,
        "Interrupted telemetry finalization does not pause simulation");

    auto record{telemetry.take_finalized_run()};
    if (!ml::simulation_tests::expect_true(record.has_value(),
                                           "Finalized manager yields one run record")) {
        return;
    }
    ml::simulation_tests::expect_false(telemetry.take_finalized_run().has_value(),
                                       "A finalized run record is yielded only once");
    ml::simulation_tests::expect_equal(record->completion.reason,
                                       ml::simulation::LevelTelemetryRunEndReason::WorldEnd,
                                       "Completion preserves its end reason");
    ml::simulation_tests::expect_equal(record->completion.completed_ticks,
                                       simulation.get_clock().get_completed_ticks(),
                                       "Completion preserves completed ticks");
    auto const& realtime{record->completed_ticks_by_real_time};
    ml::simulation_tests::expect_true(
        realtime.num() >= 3, "Recorder emits start, periodic, and final realtime mappings");
    ml::simulation_tests::expect_equal(
        realtime.value_at(0), std::uint64_t{0}, "Realtime mapping starts at tick zero");
    ml::simulation_tests::expect_equal(realtime.last_value(),
                                       simulation.get_clock().get_completed_ticks(),
                                       "Realtime mapping ends at the completed tick");

    auto const& series{record->tick_series};
    ml::simulation_tests::expect_equal(
        series.active_entities.num(), 1, "An unchanged entity count is only stored once");
    ml::simulation_tests::expect_equal(series.requested_time_scale.num(),
                                       2,
                                       "Requested time scale is only stored when it changes");
    ml::simulation_tests::expect_equal(
        series.requested_time_scale.last_time(),
        time_scale_change_tick,
        "Changed requested time scale is indexed by its first simulation tick");
    ml::simulation_tests::expect_equal(
        series.requested_time_scale.last_value(), 4.0, "Changed requested time scale is retained");
}
