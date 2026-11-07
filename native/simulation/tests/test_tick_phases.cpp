#include "support/mission_test_support.h"
#include "support/simulation_checks.h"
#include <ioj/column_math.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <array>

namespace ioj::sim::tests::tick_phases {
auto make_world() -> LevelSimInitData {
    LevelSimInitData data{};
    data.clock_settings.tick_rate = ioj::Frequency::make(10.0);
    data.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.capital_ships.fighter_spawn_slots = 0;
    data.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(8);
    data.collision.overlap_response.damage_per_overlap_detection = ioj::Positive<std::int32_t>{25};

    for (auto const type : EnumTraits<EntityType>::values) {
        data.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }

    return data;
}

void add_moving_player(LevelSimInitData& data, Vector3d const location = {}) {
    player::PlayerSpawnData spawn{};
    spawn.transform.location = location;
    spawn.flight_models.initial_slot = player::FlightModelSlot::Fourth;
    auto& gunship{
        player::flight_model_profile(spawn.flight_models, player::FlightModelSlot::Fourth)};
    gunship.config.translation.right.manual.response.mode = player::ResponseMode::Direct;
    gunship.config.translation.right.normal.positive_speed_limit = NonNegativeFinite<float>{1000.f};
    gunship.config.translation.right.normal.negative_speed_limit = NonNegativeFinite<float>{1000.f};
    spawn.health = {Health{100}, Health{100}};
    spawn.team = Team::Green;

    add_player_spawn(data, spawn);
}

void schedule_turret(LevelSimInitData& data,
                     Vector3f const location,
                     SimTick const tick = SimTick{1}) {
    auto& definitions{data.level_events};
    auto const payload_offset{definitions.turret_spawns.num()};
    definitions.turret_spawns.add_defaulted(1);
    auto const turret_spawns{definitions.turret_spawns.view(payload_offset, 1)};
    turret_spawns.entity_indices()[0] = data.entity_count++;
    set_vector(turret_spawns.view_locations(), 0, location);
    set_rotation(turret_spawns.view_rotations(), 0, {});
    turret_spawns.teams()[0] = Team::Green;
    turret_spawns.healths()[0] = Health{100};
    turret_spawns.laser_damages()[0] = 0;

    add_timed_spawn(data, EntityType::Turret, payload_offset, 1, tick);
}

void queue_capital_laser(LevelSim& simulation) {
    lasers::LaserSpawnRequests shot{};
    tests::add_laser_spawn(shot,
                           {{290.f, 100.f, 0.f}},
                           {},
                           {},
                           25,
                           1000.f,
                           10000.f,
                           simulation.capital_ships().get_entities().entity_ids()[0],
                           {Team::White, EntityType::CapitalShip});
    LevelSimTestAccess::queue_laser_spawns(simulation, shot.const_view());
}

TEST(TickPhases, CompletedBurstsStopUntilTheNextFirePress) {
    for (auto const rate : {ShipFireRate::Single, ShipFireRate::Burst3}) {
        auto data{make_world()};
        add_moving_player(data);
        data.player->laser_fire_rate = rate;
        data.player->config.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(0.05f);

        LevelSim simulation{std::move(data)};
        simulation.finish_initialisation();
        simulation.start();
        auto commands{simulation.player_ship_commands()};
        commands->start_fire_laser();
        simulation.advance(1.0);

        auto const shots{rate == ShipFireRate::Single ? 1u : 3u};
        EXPECT_EQ(simulation.lasers().get_number_spawned(), shots);
        EXPECT_EQ(simulation.player_ship_simulation()->laser_firing_mode, LaserFiringState::idle);
        simulation.advance(1.0);
        EXPECT_EQ(simulation.lasers().get_number_spawned(), shots);

        commands->start_fire_laser();
        simulation.advance(1.0);
        EXPECT_EQ(simulation.lasers().get_number_spawned(), shots * 2);
    }
}

TEST(TickPhases, AutomaticFireContinuesUntilReleased) {
    auto data{make_world()};
    add_moving_player(data);
    data.player->laser_fire_rate = ShipFireRate::FullAuto;
    data.player->config.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(0.05f);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto commands{simulation.player_ship_commands()};
    commands->start_fire_laser();
    simulation.advance(1.0);
    EXPECT_GT(simulation.lasers().get_number_spawned(), 3u);
    EXPECT_EQ(simulation.player_ship_simulation()->laser_firing_mode, LaserFiringState::burst);

    commands->stop_fire_laser();
    simulation.advance(1.0);
    auto const shots{simulation.lasers().get_number_spawned()};
    simulation.advance(1.0);
    EXPECT_EQ(simulation.lasers().get_number_spawned(), shots);
}

TEST(TickPhases, AuthoredSpawnHasPhysicalPresenceBeforeItsFirstThinking) {
    auto data{make_world()};
    add_capital_spawn(data, {}, Team::White, 60.f, 60.f, 100);
    schedule_turret(data, {});

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};

    auto const& ledger{simulation.entity_ledger()};

    auto const& queries{simulation.spatial_query_manager()};

    EXPECT_EQ(ledger.count_alive(), 1);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);

    simulation.start();
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    simulation.advance(clk_period);

    auto const view{simulation.read_access()};
    auto const& turrets{view.turrets().get_entities()};
    ASSERT_EQ(turrets.num(), 1);
    auto const turret{turrets.entity_ids()[0]};
    EXPECT_EQ(observe_entity(simulation, turret)->health.value(), 75);
    EXPECT_EQ(simulation.capital_ships()
                  .healths()
                  .health(simulation.capital_ships().get_entities().entity_ids()[0])
                  .value(),
              75);
    EXPECT_EQ(simulation.lasers().get_number_spawned(), 0);

    auto const events{queries.aabb_overlap_events()};
    EXPECT_EQ(events.entity_entity_overlaps.num(), 1);

    simulation.advance(clk_period);

    EXPECT_EQ(observe_entity(simulation, turret)->health.value(), 75);
    EXPECT_EQ(ledger.count_alive(), 2);
    EXPECT_EQ(simulation.turrets().num_instances(), 1);
}

TEST(TickPhases, CarrierSpawnIsDeferredAndParticipatesInLaunchOverlaps) {
    auto data{make_world()};

    add_capital_spawn(data, {}, Team::Green, 0.f, 60.f, 100);
    add_capital_spawn(data, {{2000.f, 0.f, 0.f}}, Team::White, 60.f, 60.f, 100);
    data.capital_ships.fighter_spawn_slots = 1;
    data.capital_ships.fighter_spawn_slots_relative_transforms = {{}};
    data.fighters.health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.fighters.speed = 0.f;

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};

    simulation.start();
    simulation.advance(clk_period);

    EXPECT_EQ(simulation.fighters().num_instances(), 0);
    simulation.advance(clk_period);

    auto const view{simulation.read_access()};
    auto const& fighters{view.fighters().entities()};
    ASSERT_EQ(fighters.num(), 1);
    auto const fighter{fighters.entity_ids()[0]};
    EXPECT_TRUE(entity_is_alive(simulation, fighter));
    EXPECT_EQ(observe_entity(simulation, fighter)->health.value(), 75);

    auto const parent{simulation.capital_ships().get_entities().entity_ids()[0]};
    EXPECT_EQ(observe_entity(simulation, parent)->health.value(), 75);

    auto const& queries{simulation.spatial_query_manager()};

    EXPECT_EQ(queries.aabb_overlap_events().entity_entity_overlaps.num(), 1);
}

TEST(TickPhases, ThinkingFireIsDeferredWithActionMovementSnapshot) {
    auto data{make_world()};
    add_moving_player(data);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};

    auto commands{simulation.player_ship_commands()};
    commands->set_right_input(1.f);
    commands->start_fire_laser();

    auto const& player{*simulation.player_ship_simulation()};

    EXPECT_DOUBLE_EQ(player.physical_state().transform.location.y(), 0.0);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);

    simulation.start();
    simulation.advance(clk_period);

    EXPECT_NEAR(player.physical_state().transform.location.y(), 100.0, 0.001);

    EXPECT_EQ(simulation.read_access().lasers().get_entities().num(), 0);
    simulation.advance(clk_period);

    auto const lasers{simulation.read_access().lasers().get_entities()};
    ASSERT_EQ(lasers.num(), 1);
    EXPECT_NEAR(lasers.view_locations().ys()[0], 200.f, 0.001f);
    EXPECT_FLOAT_EQ(lasers.view_velocities().ys()[0], 1000.f);
}

TEST(TickPhases, ActionMovementIsVisibleToSameTickOverlapDetection) {
    auto data{make_world()};
    add_moving_player(data);
    add_capital_spawn(data, {{0.f, 100.f, 0.f}}, Team::White, 0.f, 60.f, 100);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};
    simulation.player_ship_commands()->set_right_input(1.f);

    simulation.start();
    simulation.advance(clk_period);

    auto const* const player{simulation.player_ship_simulation()};
    EXPECT_NEAR(player->physical_state().transform.location.y(), 100.0, 0.001);
    EXPECT_EQ(simulation.spatial_query_manager().aabb_overlap_events().entity_entity_overlaps.num(),
              1);
    EXPECT_EQ(player->health().health.value(), 75);
}

TEST(TickPhases, AuthoredSpawnCanBeHitOnItsDispatchTick) {
    auto data{make_world()};
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::White, 60.f, 60.f, 100);
    schedule_turret(data, {{500.f, 100.f, 0.f}}, SimTick{1});

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    queue_capital_laser(simulation);

    simulation.start();

    auto const period{simulation.clock().tick_period()};
    simulation.advance(period * 2.0);

    ASSERT_EQ(simulation.turrets().num_instances(), 1);
    auto const turret{simulation.read_access().turrets().get_entities().entity_ids()[0]};
    EXPECT_EQ(observe_entity(simulation, turret)->health.value(), 75);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);

    simulation.advance(period);

    EXPECT_EQ(observe_entity(simulation, turret)->health.value(), 75);
}

TEST(TickPhases, DelayedObjectiveSeesResolvedDeathWithoutDuplicateOverlaps) {
    auto data{make_world()};
    data.collision.overlap_response.damage_per_overlap_detection = ioj::Positive<std::int32_t>{100};
    auto const capital_index{add_capital_spawn(data, {}, Team::White, 60.f, 60.f, 1000)};
    schedule_turret(data, {});
    auto const turret_index{data.level_events.turret_spawns.const_view().entity_indices()[0]};
    levels::ObjectiveDefinition objective{.team = Team::White};
    objective.expressions = {
        {ioj::levels::TimeElapsedExpr{.seconds = ioj::NonNegativeFinite<double>::make(10)}},
        destroyed(capital_index),
        destroyed(turret_index),
        {ioj::levels::AnyExpr{.children = {1, 2}}}};
    objective.roots[ioj::ObjectiveOutcome::Passed] = 0;
    objective.roots[ioj::ObjectiveOutcome::Failed] = 3;
    install_mission(data, std::move(objective));

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};

    auto const& ledger{simulation.entity_ledger()};
    auto const& capitals{simulation.capital_ships()};

    simulation.start();
    simulation.advance(clk_period);
    EXPECT_TRUE(simulation.turrets().frame_changes().empty());
    simulation.advance(clk_period);

    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    auto const& turret_view{simulation.read_access().turrets()};
    auto const changes{turret_view.frame_changes()};
    ASSERT_EQ(changes.size(), 2);
    EXPECT_EQ(changes[0].kind, EntityFrameChangeKind::Spawn);
    EXPECT_EQ(changes[1].kind, EntityFrameChangeKind::RemoveSwap);
    EXPECT_EQ(changes[0].id, changes[1].id);
    EXPECT_EQ(turret_view.death_locations().size(), 1);
    auto const dead_id{changes[1].id};
    expect_entity_unregistered(simulation, dead_id);
    EXPECT_EQ(ledger.count_alive(), 1);
    EXPECT_EQ(capitals.healths().health(capitals.get_entities().entity_ids()[0]).value(), 900);
    EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()),
              ObjectiveOutcome::Failed);

    auto const& queries{simulation.spatial_query_manager()};
    EXPECT_EQ(queries.aabb_overlap_events().entity_entity_overlaps.num(), 1);

    simulation.advance(clk_period);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    expect_entity_unregistered(simulation, dead_id);
    EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()),
              ObjectiveOutcome::Failed);
}

TEST(TickPhases, ExistingProjectilesUsePreMovementTargetsAndBoundsAdvanceAfterward) {
    auto data{make_world()};
    add_moving_player(data, {500.0, 0.0, 0.0});
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::White, 60.f, 60.f, 100);

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const& laser_sim{simulation.lasers()};
    simulation.player_ship_commands()->set_right_input(1.f);

    queue_capital_laser(simulation);

    simulation.start();

    auto const period{simulation.clock().tick_period()};
    simulation.advance(period);

    auto const* player{simulation.player_ship_simulation()};
    EXPECT_EQ(player->health().health.value(), 100);
    EXPECT_EQ(laser_sim.num_instances(), 1);

    simulation.advance(period);

    EXPECT_EQ(player->health().health.value(), 75);
    EXPECT_EQ(laser_sim.num_instances(), 0);
    EXPECT_NEAR(player->physical_state().transform.location.y(), 200.0, 0.001);

    auto const& queries{simulation.spatial_query_manager()};

    auto const bounds{queries.entity_collision_bounds()};
    auto const ids{bounds.entity_ids()};
    auto const found{std::ranges::find(ids, player->unique_entity_id)};
    ASSERT_NE(found, ids.end());
    auto const row{static_cast<std::uint32_t>(found - ids.begin())};
    EXPECT_NEAR(bounds.view_min_points()[row].y(), 190.f, 0.001f);
    EXPECT_NEAR(bounds.view_max_points()[row].y(), 210.f, 0.001f);
}

TEST(TickPhases, AcceptedFireSurvivesShooterDeathAndDeathCannotBeHealed) {
    auto data{make_world()};
    add_moving_player(data);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};
    auto const* player{simulation.player_ship_simulation()};
    simulation.player_ship_commands()->start_fire_laser();
    DirectDamageEvents damage;
    damage.add(player->unique_entity_id, 100, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.start();
    simulation.advance(clk_period);
    EXPECT_TRUE(is_dead(player->health().health));
    EXPECT_EQ(simulation.lasers().get_number_spawned(), 0);
    simulation.player_ship_commands()->add_health(Health{100});
    EXPECT_TRUE(is_dead(player->health().health));
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.lasers().get_number_spawned(), 1);
    EXPECT_EQ(simulation.lasers().num_instances(), 1);
    EXPECT_EQ(observe_entity_row(simulation, player->unique_entity_id),
              EntityInstanceHandle::invalid_value);
    EXPECT_FALSE(entity_is_alive(simulation, player->unique_entity_id));
    EXPECT_FALSE(observe_entity(simulation, player->unique_entity_id));
}

TEST(TickPhases, ShortLivedProjectileSweepsItsRemainingLifetimeFromTheMuzzle) {
    auto data{make_world()};
    add_capital_spawn(data, {{40.f, 0.f, 0.f}}, Team::White, 60.f, 60.f, 100);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};
    lasers::LaserSpawnRequests shot;
    tests::add_laser_spawn(
        shot, {}, {}, {}, 25, 1000.f, 50.f, {}, {Team::Green, EntityType::PlayerShip});
    LevelSimTestAccess::queue_laser_spawns(simulation, shot.const_view());
    simulation.start();
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.read_access()
                  .capitals()
                  .healths()
                  .health(simulation.read_access().capitals().get_entities().entity_ids()[0])
                  .value(),
              75);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_EQ(simulation.read_access().lasers().get_entities().num(), 0);
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.read_access().lasers().get_entities().num(), 0);
}

TEST(TickPhases, CapitalDeathIsPublishedBeforeMissionEvaluation) {
    // Independent scenario worlds mutate storage and publish new views during each run.
    // NOLINTBEGIN(ioj-loop-view-accessor-call)
    for (auto const kill_tick : {1, 2}) {
        auto data{make_world()};

        auto const first{
            add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::Green, 0.f, 60.f, 100)};
        auto const second{
            add_capital_spawn(data, {{2000.f, 0.f, 0.f}}, Team::White, 0.f, 60.f, 100)};
        data.capital_ships.fighter_spawn_slots = 1;
        data.capital_ships.fighter_spawn_slots_relative_transforms = {
            {.location = {0.0, 500.0, 0.0}}};

        levels::ObjectiveDefinition objective{.team = Team::White};
        objective.expressions = {
            destroyed(first),
            {ioj::levels::EntityKillsReachedExpr{.entities = {mission_entity(second)}, .kills = 1}},
            {ioj::levels::AllExpr{.children = {0, 1}}}};
        objective.roots[ioj::ObjectiveOutcome::Passed] = 2;
        install_mission(data, std::move(objective));

        LevelSim simulation{std::move(data)};
        simulation.finish_initialisation();

        auto const& ledger{simulation.entity_ledger()};
        auto const& capitals{simulation.capital_ships()};
        auto const& fighter_sim{simulation.fighters()};

        simulation.start();

        auto const period{simulation.clock().tick_period()};
        if (kill_tick == 2) {
            simulation.advance(period);

            ASSERT_EQ(fighter_sim.num_instances(), 0);
        }

        auto const victim{capitals.get_entities().entity_ids()[0]};
        auto const killer{capitals.get_entities().entity_ids()[1]};

        DirectDamageEvents damage{};
        damage.add(victim, 100, killer);

        LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());

        EXPECT_TRUE(entity_is_alive(simulation, victim));
        EXPECT_EQ(observe_entity(simulation, victim)->health.value(), 100);

        simulation.advance(period);

        EXPECT_FALSE(entity_is_alive(simulation, victim));
        EXPECT_EQ(fighter_sim.num_instances(), kill_tick == 1 ? 0 : 2);
        EXPECT_EQ(capitals.num_instances(), 1);
        auto const& capital_view{simulation.read_access().capitals()};
        auto const changes{capital_view.frame_changes()};
        ASSERT_FALSE(changes.empty());
        EXPECT_EQ(changes.back().kind, EntityFrameChangeKind::RemoveSwap);
        EXPECT_EQ(changes.back().id, victim);
        ASSERT_EQ(ledger.frame_deaths(EntityType::CapitalShip).size(), 1);
        EXPECT_EQ(observe_entity_row(simulation, victim), EntityInstanceHandle::invalid_value);
        EXPECT_EQ(observe_entity_row(simulation, killer), 0);
        EXPECT_EQ(ledger.count_alive(), kill_tick == 1 ? 1 : 3);
        if (kill_tick == 1) {
            EXPECT_FALSE(
                simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()));
            simulation.advance(period);
        }
        EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()),
                  ObjectiveOutcome::Passed);
    }

    // NOLINTEND(ioj-loop-view-accessor-call)
    auto data{make_world()};
    for (auto const x : {-2000.f, 0.f, 2000.f}) {
        add_capital_spawn(data, {{x, 0.f, 0.f}}, Team::White, 60.f, 60.f, 100);
    }
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const initial{simulation.read_access().capitals().get_entities()};
    std::vector<EntityId> visual_ids{initial.entity_ids().begin(), initial.entity_ids().end()};
    auto const clk_period{simulation.clock().tick_period()};
    auto const survivor_id{initial.entity_ids()[2]};
    DirectDamageEvents damage;
    damage.add(initial.entity_ids()[0], 100, {});
    damage.add(initial.entity_ids()[1], 100, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.start();
    simulation.advance(clk_period * 2.0);
    auto const& final{simulation.read_access().capitals()};
    ASSERT_EQ(final.frame_changes().size(), 2);
    EXPECT_EQ(simulation.entity_ledger().frame_deaths(EntityType::CapitalShip).size(), 2);
    for (auto const& change : final.frame_changes()) {
        EXPECT_EQ(change.kind, EntityFrameChangeKind::RemoveSwap);
        ASSERT_LT(static_cast<std::size_t>(change.index), visual_ids.size());
        EXPECT_EQ(visual_ids[change.index], change.id);
        visual_ids[change.index] = visual_ids.back();
        visual_ids.pop_back();
    }
    ASSERT_EQ(visual_ids.size(), 1);
    ASSERT_EQ(final.get_entities().num(), 1);
    EXPECT_EQ(visual_ids[0], final.get_entities().entity_ids()[0]);
    EXPECT_EQ(observe_entity_row(simulation, survivor_id), 0);
    EXPECT_TRUE(observe_live_entity(simulation, survivor_id));
}

}
