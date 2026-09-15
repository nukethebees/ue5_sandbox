#include <ioj/sim/testing/level_sim_test_access.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests::tick_phases {
auto make_world() -> LevelSimInitData {
    LevelSimInitData data{};
    data.clock_settings.tick_rate = 10.0;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.capital_ships.fighter_spawn_slots = 0;
    data.lasers.n_preallocated_instances = 8;
    data.overlap_response.damage_per_overlap_detection = 25;

    auto const count{data.entity_bounds.num()};
    for (std::int32_t index{}; index < count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }

    return data;
}

void add_moving_player(LevelSimInitData& data, ml::Vector3d const location = {}) {
    player::PlayerSpawnData spawn{};
    spawn.transform.location = location;
    spawn.config.cruise_speed = 0.f;
    spawn.config.lateral_adjustment_speed = 1000.f;
    spawn.health = {100, 100};
    spawn.team = Team::Green;

    add_player_spawn(data, spawn);
}

void schedule_turret(LevelSimInitData& data, Vector3f const location, SimTick const tick = 1) {
    auto& schedule{data.level_events.schedule};
    schedule.execution_ticks = {tick};
    schedule.event_group_counts = {{}};
    schedule.turret_spawns.add_defaulted(1);
    schedule.turret_spawns.entity_indices[0] = data.level_events.initialisation.entity_count++;
    schedule.turret_spawns.locations.set(0, location);
    schedule.turret_spawns.rotations.set(0, {});
    schedule.turret_spawns.teams[0] = Team::Green;
    schedule.turret_spawns.healths[0] = 100;
    schedule.turret_spawns.laser_damages[0] = 0;

    ASSERT_TRUE(schedule.add_spawn_group(EntityType::Turret, 0, 1));
}

TEST(TickPhases, AuthoredSpawnHasPhysicalPresenceBeforeItsFirstThinking) {
    auto data{make_world()};
    add_capital_spawn(data, {}, Team::White, -1, 60.f, 60.f, 100);
    schedule_turret(data, {});

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& registry{simulation.get_entity_registry()};

    auto const& queries{simulation.get_spatial_query_manager()};

    EXPECT_EQ(registry.count_alive(), 1);
    EXPECT_EQ(simulation.get_turrets().get_num_instances(), 0);

    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());

    auto const view{simulation.get_read_view()};
    auto const& turrets{view.turrets.entities};
    ASSERT_EQ(turrets.num(), 1);
    auto const turret{turrets.handles[0]};
    EXPECT_EQ(registry.get_health(turret), 75);
    EXPECT_EQ(registry.get_health(simulation.get_capital_ships().get_handle(0)), 75);
    EXPECT_EQ(simulation.get_lasers().get_number_spawned(), 0);
    EXPECT_TRUE(registry.get_moved_entities_this_tick().empty());

    auto const events{queries.get_collision_system().get_aabb_overlap_events()};
    EXPECT_EQ(events.entity_entity_overlaps.num(), 1);

    simulation.advance(simulation.get_clock().get_tick_period());

    EXPECT_EQ(registry.get_health(turret), 75);
    EXPECT_EQ(registry.count_alive(), 2);
    EXPECT_EQ(simulation.get_turrets().get_num_instances(), 1);
}

TEST(TickPhases, CarrierSpawnIsQueryableWithoutInitiatingLaunchOverlaps) {
    auto data{make_world()};

    auto const first{add_capital_spawn(data, {}, Team::Green, -1, 0.f, 60.f, 100)};
    auto const second{
        add_capital_spawn(data, {{2000.f, 0.f, 0.f}}, Team::White, -1, 60.f, 60.f, 100)};
    data.level_events.initial_spawns.capital_spawns.target_entity_indices = {second, first};
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {{}};
    data.fighters.health = 100;

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& registry{simulation.get_entity_registry()};

    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());

    auto const view{simulation.get_read_view()};
    auto const& fighters{view.fighters.entities};
    ASSERT_EQ(fighters.num(), 1);
    auto const fighter{fighters.entity_handles[0]};
    EXPECT_TRUE(registry.is_valid_alive(fighter));
    EXPECT_EQ(registry.get_health(fighter), 100);

    auto const parent{simulation.get_capital_ships().get_handle(0)};
    EXPECT_EQ(registry.get_health(parent), 100);

    auto const& queries{simulation.get_spatial_query_manager()};

    auto const hit{queries.trace_closest({{-20.f, 0.f, 0.f}}, {{20.f, 0.f, 0.f}}, parent)};
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.entity, fighter);
    EXPECT_EQ(queries.get_collision_system().get_aabb_overlap_events().entity_entity_overlaps.num(),
              0);
}

TEST(TickPhases, ThinkingFireUsesPredictedActionMovement) {
    auto data{make_world()};
    add_moving_player(data);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto* commands{simulation.get_player_ship_commands()};
    commands->set_lateral_move_input(1.f);
    commands->start_fire_laser();

    auto const& player{*simulation.get_player_ship_simulation()};

    EXPECT_DOUBLE_EQ(player.get_movement_state().transform.location.y, 0.0);
    EXPECT_EQ(simulation.get_lasers().get_num_instances(), 0);

    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());

    EXPECT_NEAR(player.get_movement_state().transform.location.y, 100.0, 0.001);

    auto const lasers{simulation.get_read_view().lasers.entities};
    ASSERT_EQ(lasers.num(), 1);
    EXPECT_NEAR(lasers.locations.ys[0], 100.f, 0.001f);
    EXPECT_FLOAT_EQ(lasers.velocities.ys[0], 1000.f);
}

TEST(TickPhases, AuthoredSpawnIsNotHitByEarlierActionProjectiles) {
    auto data{make_world()};
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::White, -1, 60.f, 60.f, 100);
    schedule_turret(data, {{500.f, 100.f, 0.f}}, 2);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& registry{simulation.get_entity_registry()};

    lasers::SpawnRequests shot{};
    shot.add({{290.f, 100.f, 0.f}},
             {},
             {},
             25,
             1000.f,
             10000.f,
             simulation.get_capital_ships().get_handle(0),
             {Team::White, EntityType::CapitalShip});

    LevelSimTestAccess::queue_laser_spawns(simulation, shot.get_const_view());

    simulation.start();

    auto const period{simulation.get_clock().get_tick_period()};
    simulation.advance(period * 2.0);

    ASSERT_EQ(simulation.get_turrets().get_num_instances(), 1);
    auto const turret{simulation.get_read_view().turrets.entities.handles[0]};
    EXPECT_EQ(registry.get_health(turret), 100);
    EXPECT_EQ(simulation.get_lasers().get_num_instances(), 1);
    EXPECT_EQ(simulation.get_spatial_query_manager()
                  .trace_closest({{480.f, 100.f, 0.f}}, {{520.f, 100.f, 0.f}})
                  .entity,
              turret);

    simulation.advance(period);

    EXPECT_EQ(registry.get_health(turret), 75);
}

TEST(TickPhases, SpawnMissionEventsSeeSameTickResolvedDeathWithoutDuplicateOverlaps) {
    auto data{make_world()};
    data.overlap_response.damage_per_overlap_detection = 100;
    auto const capital_index{add_capital_spawn(data, {}, Team::White, -1, 60.f, 60.f, 1000)};
    schedule_turret(data, {});
    auto const turret_index{data.level_events.schedule.turret_spawns.entity_indices[0]};
    ASSERT_TRUE(data.level_events.schedule.add_mission_group(LevelMissionEventType::MustSurvive,
                                                             std::array{turret_index}));

    auto& mission{data.level_events.initialisation.mission.emplace()};
    mission.mode = levels::LevelMissionMode::SurviveTime;
    mission.time_limit_seconds = 10.f;
    mission.save_results = false;
    mission.must_survive_entity_indices = {capital_index};

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& registry{simulation.get_entity_registry()};
    auto const& capitals{simulation.get_capital_ships()};

    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());

    EXPECT_EQ(simulation.get_turrets().get_num_instances(), 0);
    EXPECT_EQ(registry.count_alive(), 1);
    EXPECT_EQ(registry.get_health(capitals.get_handle(0)), 900);
    EXPECT_EQ(simulation.get_mission_manager().get_mission_state(), MissionState::Failed);

    auto const& queries{simulation.get_spatial_query_manager()};
    EXPECT_EQ(queries.get_collision_system().get_aabb_overlap_events().entity_entity_overlaps.num(),
              1);
    EXPECT_EQ(queries.trace_closest({{-20.f, 0.f, 0.f}}, {{20.f, 0.f, 0.f}}).entity,
              capitals.get_handle(0));
}

TEST(TickPhases, ExistingProjectilesUsePreMovementTargetsAndQueriesAdvanceAfterward) {
    auto data{make_world()};
    add_moving_player(data, {500.0, 0.0, 0.0});
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::White, -1, 60.f, 60.f, 100);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& laser_sim{simulation.get_lasers()};
    simulation.get_player_ship_commands()->set_lateral_move_input(1.f);

    lasers::SpawnRequests shot{};
    shot.add({{290.f, 100.f, 0.f}},
             {},
             {},
             25,
             1000.f,
             10000.f,
             simulation.get_capital_ships().get_handle(0),
             {Team::White, EntityType::CapitalShip});

    LevelSimTestAccess::queue_laser_spawns(simulation, shot.get_const_view());

    simulation.start();

    auto const period{simulation.get_clock().get_tick_period()};
    simulation.advance(period);

    auto const* player{simulation.get_player_ship_simulation()};
    EXPECT_EQ(player->health.health, 100);
    EXPECT_EQ(laser_sim.get_num_instances(), 1);

    simulation.advance(period);

    EXPECT_EQ(player->health.health, 75);
    EXPECT_EQ(laser_sim.get_num_instances(), 0);
    EXPECT_NEAR(player->get_movement_state().transform.location.y, 200.0, 0.001);

    auto const& queries{simulation.get_spatial_query_manager()};

    auto const moved_hit{queries.trace_closest({{480.f, 200.f, 0.f}}, {{520.f, 200.f, 0.f}})};
    EXPECT_TRUE(moved_hit.hit);
    EXPECT_EQ(moved_hit.entity, player->registry_handle);
    EXPECT_FALSE(queries.trace_closest({{480.f, 100.f, 0.f}}, {{520.f, 100.f, 0.f}}).hit);
}

TEST(TickPhases, CapitalDeathPublishesExistingAndNewChildDeathsBeforeMissionEvaluation) {
    for (auto const kill_tick : {1, 2}) {
        auto data{make_world()};

        auto const first{
            add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::Green, -1, 0.f, 60.f, 100)};
        auto const second{
            add_capital_spawn(data, {{2000.f, 0.f, 0.f}}, Team::White, -1, 0.f, 60.f, 100)};
        data.level_events.initial_spawns.capital_spawns.target_entity_indices = {second, first};
        data.capital_ships.fighter_spawn_slots = 1;
        data.capital_ships.fighter_spawn_slots_relative_transforms = {
            {.location = {0.0, 500.0, 0.0}}};

        auto& mission{data.level_events.initialisation.mission.emplace()};
        mission.mode = levels::LevelMissionMode::KillEnemies;
        mission.kill_count = 1;
        mission.save_results = false;
        mission.hero_entity_indices = {second};
        mission.required_kill_entity_indices = {first};

        LevelSim simulation{std::move(data)};
        simulation.finish_initialisation();

        auto const& registry{simulation.get_entity_registry()};
        auto const& capitals{simulation.get_capital_ships()};
        auto const& fighter_sim{simulation.get_fighters()};

        auto const& queries{simulation.get_spatial_query_manager()};

        simulation.start();

        auto const period{simulation.get_clock().get_tick_period()};
        if (kill_tick == 2) {
            simulation.advance(period);

            ASSERT_EQ(fighter_sim.get_num_instances(), 2);
        }

        auto const victim{capitals.get_handle(0)};
        auto const killer{capitals.get_handle(1)};

        DirectDamageEvents damage{};
        damage.add(victim, 100, killer);

        LevelSimTestAccess::queue_direct_damage_events(simulation, damage.get_const_view());

        EXPECT_TRUE(registry.is_valid_alive(victim));
        EXPECT_EQ(registry.get_health(victim), 100);
        EXPECT_TRUE(queries.trace_closest({{-2020.f, 0.f, 0.f}}, {{-1980.f, 0.f, 0.f}}).hit);

        simulation.advance(period);

        EXPECT_FALSE(registry.is_valid_alive(victim));
        EXPECT_EQ(fighter_sim.get_num_instances(), 1);
        EXPECT_EQ(registry.count_alive(), 2);
        EXPECT_EQ(simulation.get_mission_manager().get_mission_state(), MissionState::Succeeded);
        EXPECT_FALSE(queries.trace_closest({{-2020.f, 0.f, 0.f}}, {{-1980.f, 0.f, 0.f}}).hit);
        EXPECT_FALSE(queries.trace_closest({{-2020.f, 500.f, 0.f}}, {{-1980.f, 500.f, 0.f}}).hit);
    }
}

}
