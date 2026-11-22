#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <array>

namespace ioj::sim::tests::tick_phases {
auto make_world() -> SimulationScenario {
    SimulationScenario data{};
    data.runtime.clock_settings.tick_rate = ioj::Frequency::make(10.0);
    data.runtime.collision.grid_spec = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(8);
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        ioj::Positive<std::int32_t>{25};

    for (auto const type : EnumTraits<EntityType>::values) {
        data.runtime.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }

    return data;
}

void add_moving_player(SimulationScenario& data,
                       Vector3d const location = {},
                       float const cruise_speed = 0.f) {
    player::PlayerSpawnData spawn{};
    spawn.transform.location = location;
    spawn.config.cruise_speed = NonNegativeFinite<float>::make(cruise_speed);
    spawn.config.response_settling_time = PositiveFinite<float>{0.005f};
    if (cruise_speed > 0.f) {
        spawn.transform.rotation = to_quaternion(Rotator3d{0.0, 90.0, 0.0});
        spawn.middle_socket.rotation = to_quaternion(Rotator3d{0.0, -90.0, 0.0});
    }
    spawn.health = {Health{100}, Health{100}};
    spawn.team = Team::Green;

    add_player_spawn(data, spawn);
}

auto schedule_turret(SimulationScenario& data,
                     Vector3f const location,
                     SimTick const tick = SimTick{1}) -> std::uint32_t {
    auto const index{add_turret_spawn(data,
                                      location,
                                      {},
                                      Team::Green,
                                      settings::PositiveHealth{Health{100}},
                                      NonNegative<std::int32_t>{0})};
    data.entities[index].spawn_time_seconds = NonNegativeFinite<double>::make(
        static_cast<double>(tick.value()) / data.runtime.clock_settings.tick_rate.hz());
    return index;
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
        data.runtime.player->laser_fire_rate = rate;
        data.runtime.player->config.laser.fire_cooldown =
            ::ioj::NonNegativeFinite<float>::make(0.05f);

        LevelSim simulation{std::move(data).compile()};

        simulation.start();
        auto commands{simulation.player_ship_commands()};
        commands->start_fire_laser();
        simulation.advance(1.0);

        auto const shots{rate == ShipFireRate::Single ? 1u : 3u};
        EXPECT_EQ(simulation.read_access().frame_events().player.size(), shots);
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
    data.runtime.player->laser_fire_rate = ShipFireRate::FullAuto;
    data.runtime.player->config.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(0.05f);

    LevelSim simulation{std::move(data).compile()};

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
    add_capital_spawn(data,
                      {},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    schedule_turret(data, {});

    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};

    auto const& ledger{simulation.entity_ledger()};

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
    EXPECT_EQ(simulation.entity_tables()
                  .health.const_view<EntityType::CapitalShip>()
                  .health(simulation.capital_ships().get_entities().entity_ids()[0])
                  .value(),
              75);
    EXPECT_EQ(simulation.lasers().get_number_spawned(), 0);

    simulation.advance(clk_period);

    EXPECT_EQ(observe_entity(simulation, turret)->health.value(), 75);
    EXPECT_EQ(ledger.count_alive(), 2);
    EXPECT_EQ(simulation.turrets().num_instances(), 1);
}

TEST(TickPhases, CarrierSpawnIsDeferredAndClearsTheCarrier) {
    auto data{make_world()};

    add_capital_spawn(data,
                      {},
                      Team::Green,
                      0.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    add_capital_spawn(data,
                      {{2000.f, 0.f, 0.f}},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms = {{}};
    data.runtime.fighters.health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.runtime.fighters.speed = 0.f;

    LevelSim simulation{std::move(data).compile()};

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
    EXPECT_EQ(observe_entity(simulation, fighter)->health.value(), 100);

    auto const parent{simulation.capital_ships().get_entities().entity_ids()[0]};
    EXPECT_EQ(observe_entity(simulation, parent)->health.value(), 100);
}

TEST(TickPhases, ThinkingFireIsDeferredWithActionMovementSnapshot) {
    auto data{make_world()};
    add_moving_player(data, {}, 1000.f);

    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};

    auto commands{simulation.player_ship_commands()};
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
    add_moving_player(data, {}, 1000.f);
    add_capital_spawn(data,
                      {{0.f, 100.f, 0.f}},
                      Team::White,
                      0.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));

    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};

    simulation.start();
    simulation.advance(clk_period);

    auto const* const player{simulation.player_ship_simulation()};
    EXPECT_NEAR(player->physical_state().transform.location.y(), 100.0, 0.001);
    EXPECT_EQ(player->health(simulation.entity_tables().health).health.value(), 75);
}

TEST(TickPhases, AuthoredSpawnCanBeHitOnItsDispatchTick) {
    auto data{make_world()};
    add_capital_spawn(data,
                      {{-2000.f, 0.f, 0.f}},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    schedule_turret(data, {{500.f, 100.f, 0.f}}, SimTick{1});

    LevelSim simulation{std::move(data).compile()};

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

TEST(TickPhases, DelayedEntityDeathTriggersAfterResolutionWithoutDuplicateOverlaps) {
    auto data{make_world()};
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        ioj::Positive<std::int32_t>{100};
    auto const capital_index{
        add_capital_spawn(data,
                          {},
                          Team::White,
                          60.f,
                          60.f,
                          ioj::settings::PositiveHealth::make(ioj::sim::Health{1000}))};
    auto const turret_index{schedule_turret(data, {})};
    data.logic.expressions = {{levels::EntityDestroyedExpr{data.entities[capital_index].id}},
                              {levels::EntityDestroyedExpr{data.entities[turret_index].id}},
                              {levels::AnyExpr{{0, 1}}}};
    data.logic.rules = {{2, {levels::WinEffect{Team::Green}}}};

    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};

    auto const& ledger{simulation.entity_ledger()};
    auto const& capitals{simulation.capital_ships()};

    simulation.start();
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.read_access().frame_events().turret_changes.num(), 0u);
    simulation.advance(clk_period);

    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    auto const& events{simulation.read_access().frame_events()};
    auto const changes{events.turret_changes.const_view()};
    ASSERT_EQ(changes.num(), 2);
    EXPECT_EQ(changes.kinds()[0], EntityFrameChangeKind::Spawn);
    EXPECT_EQ(changes.kinds()[1], EntityFrameChangeKind::RemoveSwap);
    EXPECT_EQ(changes.ids()[0], changes.ids()[1]);
    EXPECT_EQ(events.deaths.num(), 1);
    auto const dead_id{changes.ids()[1]};
    expect_entity_unregistered(simulation, dead_id);
    EXPECT_EQ(ledger.count_alive(), 1);
    EXPECT_EQ(simulation.entity_tables()
                  .health.const_view<EntityType::CapitalShip>()
                  .health(capitals.get_entities().entity_ids()[0])
                  .value(),
              900);
    ASSERT_TRUE(simulation.result());
    EXPECT_EQ(simulation.result()->winner, Team::Green);

    simulation.advance(clk_period);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    expect_entity_unregistered(simulation, dead_id);
    ASSERT_TRUE(simulation.result());
    EXPECT_EQ(simulation.result()->winner, Team::Green);
}

TEST(TickPhases, ExistingProjectilesUsePreMovementTargetsAndBoundsAdvanceAfterward) {
    auto data{make_world()};
    add_moving_player(data, {500.0, 0.0, 0.0}, 1000.f);
    add_capital_spawn(data,
                      {{-2000.f, 0.f, 0.f}},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));

    LevelSim simulation{std::move(data).compile()};

    auto const& laser_sim{simulation.lasers()};

    queue_capital_laser(simulation);

    simulation.start();

    auto const period{simulation.clock().tick_period()};
    simulation.advance(period);

    auto const* player{simulation.player_ship_simulation()};
    EXPECT_EQ(player->health(simulation.entity_tables().health).health.value(), 100);
    EXPECT_EQ(laser_sim.num_instances(), 1);

    simulation.advance(period);

    EXPECT_EQ(player->health(simulation.entity_tables().health).health.value(), 75);
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
    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};
    auto const* player{simulation.player_ship_simulation()};
    simulation.player_ship_commands()->start_fire_laser();
    DirectDamageEvents damage;
    damage.add(player->unique_entity_id, 100, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.start();
    simulation.advance(clk_period);
    EXPECT_TRUE(is_dead(player->health(simulation.entity_tables().health).health));
    EXPECT_EQ(simulation.lasers().get_number_spawned(), 0);
    simulation.add_player_health(Health{100});
    EXPECT_TRUE(is_dead(player->health(simulation.entity_tables().health).health));
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
    add_capital_spawn(data,
                      {{40.f, 0.f, 0.f}},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    LevelSim simulation{std::move(data).compile()};

    auto const clk_period{simulation.clock().tick_period()};
    lasers::LaserSpawnRequests shot;
    tests::add_laser_spawn(
        shot, {}, {}, {}, 25, 1000.f, 50.f, {}, {Team::Green, EntityType::PlayerShip});
    LevelSimTestAccess::queue_laser_spawns(simulation, shot.const_view());
    simulation.start();
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.read_access()
                  .entity_tables()
                  .health.const_view<EntityType::CapitalShip>()
                  .health(simulation.read_access().capitals().get_entities().entity_ids()[0])
                  .value(),
              75);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_EQ(simulation.read_access().lasers().get_entities().num(), 0);
    simulation.advance(clk_period);
    EXPECT_EQ(simulation.read_access().lasers().get_entities().num(), 0);
}

TEST(TickPhases, CapitalDeathIsPublishedBeforeLogicEvaluation) {
    // Independent scenario worlds mutate storage and publish new views during each run.
    // NOLINTBEGIN(ioj-loop-view-accessor-call)
    for (auto const kill_tick : {1, 2}) {
        auto data{make_world()};

        auto const first{
            add_capital_spawn(data,
                              {{-2000.f, 0.f, 0.f}},
                              Team::Green,
                              0.f,
                              60.f,
                              ioj::settings::PositiveHealth::make(ioj::sim::Health{100}))};
        auto const second{
            add_capital_spawn(data,
                              {{2000.f, 0.f, 0.f}},
                              Team::White,
                              0.f,
                              60.f,
                              ioj::settings::PositiveHealth::make(ioj::sim::Health{100}))};
        data.runtime.capital_ships.fighter_spawn_slots_relative_transforms = {
            {.location = {0.0, 500.0, 0.0}}};

        data.logic.expressions = {{levels::EntityDestroyedExpr{data.entities[first].id}},
                                  {levels::EntityKillsReachedExpr{{data.entities[second].id}, 1}},
                                  {levels::AllExpr{{0, 1}}}};
        data.logic.rules = {{2, {levels::WinEffect{Team::White}}}};
        if (kill_tick == 2) {
            data.logic.rules = {{2,
                                 {levels::ChangePointsEffect{Team::White, 10},
                                  levels::ChangePointsEffect{Team::Green, -3}}}};
        }

        LevelSim simulation{std::move(data).compile()};

        auto const& ledger{simulation.entity_ledger()};
        auto const& capitals{simulation.capital_ships()};
        auto const& fighter_sim{simulation.fighters()};

        simulation.start();

        auto const period{simulation.clock().tick_period()};
        if (kill_tick == 2) {
            simulation.advance(period);

            ASSERT_EQ(fighter_sim.num_instances(), 0);
            EXPECT_EQ(simulation.points(Team::White), 0);
            auto const& tasks{simulation.task_manager()};
            auto const instruction{tasks.first_instruction(tasks::TaskId{1})};
            EXPECT_EQ(tasks.instruction(instruction).data_kill(),
                      capitals.get_entities().entity_ids()[0]);
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
        auto const changes{simulation.read_access().frame_events().capital_changes.const_view()};
        ASSERT_GT(changes.num(), 0u);
        EXPECT_EQ(changes.kinds().back(), EntityFrameChangeKind::RemoveSwap);
        EXPECT_EQ(changes.ids().back(), victim);
        ASSERT_EQ(simulation.read_access().frame_events().deaths.num(), 1);
        EXPECT_EQ(observe_entity_row(simulation, victim), EntityInstanceHandle::invalid_value);
        EXPECT_EQ(observe_entity_row(simulation, killer), 0);
        EXPECT_EQ(ledger.count_alive(), kill_tick == 1 ? 1 : 3);
        if (kill_tick == 1) {
            ASSERT_TRUE(simulation.result());
            EXPECT_EQ(simulation.result()->winner, Team::White);
        } else {
            EXPECT_FALSE(simulation.result());
            EXPECT_EQ(simulation.points(Team::White), 10);
            EXPECT_EQ(simulation.points(Team::Green), -3);
            simulation.advance(period);
            EXPECT_EQ(simulation.points(Team::White), 10);
        }
    }

    // NOLINTEND(ioj-loop-view-accessor-call)
}

TEST(TickPhases, RemovalEventsReplaySwapRemovalAcrossTheCompleteFrame) {
    auto data{make_world()};
    for (auto const x : {-2000.f, 0.f, 2000.f}) {
        add_capital_spawn(data,
                          {{x, 0.f, 0.f}},
                          Team::White,
                          60.f,
                          60.f,
                          ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    }
    LevelSim simulation{std::move(data).compile()};

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
    auto const changes{simulation.read_access().frame_events().capital_changes.const_view()};
    ASSERT_EQ(changes.num(), 2);
    EXPECT_EQ(simulation.read_access().frame_events().deaths.num(), 2);
    for (std::uint32_t row{}; row < changes.num(); ++row) {
        EXPECT_EQ(changes.kinds()[row], EntityFrameChangeKind::RemoveSwap);
        ASSERT_LT(static_cast<std::size_t>(changes.indices()[row]), visual_ids.size());
        EXPECT_EQ(visual_ids[changes.indices()[row]], changes.ids()[row]);
        visual_ids[changes.indices()[row]] = visual_ids.back();
        visual_ids.pop_back();
    }
    ASSERT_EQ(visual_ids.size(), 1);
    ASSERT_EQ(final.get_entities().num(), 1);
    EXPECT_EQ(visual_ids[0], final.get_entities().entity_ids()[0]);
    EXPECT_EQ(observe_entity_row(simulation, survivor_id), 0);
    EXPECT_TRUE(observe_live_entity(simulation, survivor_id));
}

}
