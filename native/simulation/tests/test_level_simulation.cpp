#include "support/mission_test_support.h"
#include "support/simulation_checks.h"
#include <ioj/column_math.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/levels/initial_spawns.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

namespace ioj::sim::tests {

static_assert(requires(LevelSim& simulation) {
    { simulation.lasers() } -> std::same_as<lasers::Sim const&>;
    { simulation.capital_ships() } -> std::same_as<capital_ships::Sim const&>;
    { simulation.fighters() } -> std::same_as<fighters::Sim const&>;
    { simulation.turrets() } -> std::same_as<turrets::Sim const&>;
    { simulation.spinners() } -> std::same_as<spinners::Sim const&>;
    { simulation.entity_ledger() } -> std::same_as<EntityLedger const&>;
    { simulation.mission() } -> std::same_as<Mission const&>;
    { simulation.spatial_query_manager() } -> std::same_as<SpatialQueryManager const&>;
    { simulation.level_telemetry_manager() } -> std::same_as<LevelTelemetryManager const&>;
    { simulation.player_ship_simulation() } -> std::same_as<player::Sim const*>;
    { simulation.player_ship_commands() } -> std::same_as<std::optional<player::CommandInterface>>;
});

static_assert(
    std::is_const_v<std::remove_reference_t<
        decltype(std::declval<capital_ships::Sim const&>().get_entities().view_locations().xs()
                     [0])>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<fighters::Sim const&>().entities().teams()[0])>>);
static_assert(
    std::is_const_v<
        std::remove_reference_t<decltype(std::declval<turrets::Sim const&>().frame_changes()[0])>>);
static_assert(
    std::is_const_v<std::remove_reference_t<
        decltype(std::declval<lasers::Sim const&>().get_entities().lifetimes_remaining()[0])>>);

namespace {
auto make_battle() -> LevelSimInitData {
    LevelSimInitData data;
    data.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(16);
    data.capital_ships.fighter_spawn_slots = 0;
    data.level_events.initialisation.entity_count = 2;
    std::array<InitialCapital, 2> capitals{{{.entity = 0,
                                             .location = {{-1000.f, 0.f, 0.f}},
                                             .rotation = {},
                                             .team = Team::Green,
                                             .health = 100,
                                             .initial_spawn_delay = 60.f,
                                             .spawn_cooldown = 60.f},
                                            {.entity = 1,
                                             .location = {{1000.f, 0.f, 0.f}},
                                             .rotation = {},
                                             .team = Team::White,
                                             .health = 100,
                                             .initial_spawn_delay = 60.f,
                                             .spawn_cooldown = 60.f}}};
    initialise_spawns(data, capitals, {}, {});
    for (auto const type : EnumTraits<EntityType>::values) {
        data.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}

auto make_scheduled_battle() -> LevelSimInitData {
    auto data{make_battle()};
    data.capital_ships.max_health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.clock_settings.tick_rate = Frequency::make(10.0);
    auto definition{levels::authoring::DefinitionReader{}.read_level_source(R"(
        (level :id 'scheduled-battle :title "Scheduled Battle"
          :camera (camera :look-at '(hero) :offset-direction '(-1 0 0) :distance 1000)
          :mission (mission :objectives (list
            (objective :id 'primary :team 'blue :pass (entity-destroyed 'enemy))))
          :entities (list
            (entity :id 'hero :archetype 'capital-ship :team 'blue :position '(-1000 0 0))
            (entity :id 'enemy :archetype 'capital-ship :team 'red :position '(1000 0 0)
              :spawn-at 0.21))))")};
    assert(definition);
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{levels::compile_level(*definition, clock, data.capital_ships, data.turrets)};
    assert(compiled);
    data.level_events = std::move(compiled->events);
    data.mission = std::move(compiled->mission);
    data.expressions = std::move(compiled->expressions);
    data.level_metadata = std::move(compiled->level_metadata);
    return data;
}

auto make_overlap_response_battle() -> LevelSimInitData {
    auto data{make_battle()};
    data.collision.overlap_response.damage_per_overlap_detection = 50;
    std::ranges::fill(data.level_events.schedule.capital_spawns.view().healths(), Health{5000});
    add_player_spawn(data, {});
    data.player->transform.location = {-1000.0, 0.0, 0.0};
    data.player->health = {Health{150}, Health{150}};
    data.collision.entity_bounds.set_half_extents(EntityType::CapitalShip, {{100.f, 100.f, 100.f}});
    return data;
}

void add_mission(LevelSimInitData& data) {
    auto const entities{data.level_events.schedule.capital_spawns.const_view().entity_indices()};
    levels::ObjectiveDefinition objective{.team = Team::Green};
    objective.expressions = {destroyed(entities[1]),
                             {ioj::levels::EntityKillsReachedExpr{
                                 .entities = {mission_entity(entities[0])}, .kills = 1}},
                             {ioj::levels::AllExpr{.children = {0, 1}}}};
    objective.roots[ioj::ObjectiveOutcome::Passed] = 2;
    install_mission(data, std::move(objective));
}

void kill_enemy(LevelSim& simulation) {
    DirectDamageEvents events;
    events.add(simulation.capital_ships().get_entities().entity_ids()[1],
               100,
               simulation.capital_ships().get_entities().entity_ids()[0]);
    LevelSimTestAccess::queue_direct_damage_events(simulation, events.const_view());
}

void expect_health_mappings(LevelSim const& simulation) {
    auto const world{simulation.read_access()};
    auto const& table{simulation.entity_tables().health};
    auto validate = [&]<EntityType Type>(auto const& entities, HealthConstView const healths) {
        auto const direct{table.const_view<Type>()};
        EXPECT_EQ(direct.values().data(), healths.values().data());
        for (auto const id : entities.entity_ids()) {
            EXPECT_EQ(direct.health(id), healths.health(id));
        }
    };

    validate.operator()<EntityType::CapitalShip>(world.capitals().get_entities(),
                                                 world.capitals().healths());
    validate.operator()<EntityType::Fighter>(world.fighters().entities(),
                                             world.fighters().healths());
    validate.operator()<EntityType::Turret>(world.turrets().get_entities(),
                                            world.turrets().healths());
    if (auto const* const player{simulation.player_ship_simulation()}) {
        EXPECT_EQ(table.health(player->unique_entity_id), player->health().health);
    }
}
}

TEST(NativeSimulation, ScheduledSpawnsAndObjectivesUseSimulationTicks) {
    LevelSim simulation{make_scheduled_battle()};
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1)
        << "Tick-zero entities are available before finishing initialization";
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1)
        << "Only tick-zero entities exist initially";
    EXPECT_TRUE(
        !simulation.mission().outcome(sim::ObjectiveId{0}, simulation.expressions()).has_value())
        << "Future objectives prevent early completion";

    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    simulation.advance(dt);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1)
        << "A fractional authoring time rounds up to the next tick";

    simulation.advance(dt);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1)
        << "The timer resolves before the spawn is dispatched";
    simulation.advance(dt);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 2)
        << "The delayed entity spawns in the next Preparation";
    EXPECT_FALSE(
        simulation.mission().outcome(sim::ObjectiveId{0}, simulation.expressions()).has_value())
        << "Spawn alone does not complete the objective";
    kill_enemy(simulation);
    simulation.advance(dt);
    EXPECT_TRUE(simulation.mission().outcome(sim::ObjectiveId{0}, simulation.expressions()) ==
                ObjectiveOutcome::Passed)
        << "Destruction completes the objective";
}

/* **************************************** */
// Initialization and spatial queries
/* **************************************** */

TEST(NativeSimulation, BuildsAndControlsAHeadlessPlayerFromNativeDefinitionAndGeometry) {
    auto definition{levels::LevelDefinition::make(
        {.metadata = {.id = levels::LevelId{"headless-player"}, .title = "Headless player"},
         .player_entity_id = levels::EntityId{"player"},
         .entities = {{.id = levels::EntityId{"player"},
                       .archetype = levels::EntityArchetype::PlayerFighter,
                       .team = Team::Blue,
                       .position = {1200.0, 200.0, 0.0},
                       .overrides = {.health = 275}}},
         .settings = {.tick = {.tick_rate = Frequency::make(30.0)},
                      .fighters = {.speed = 1234.f}}})};
    ASSERT_TRUE(definition);
    auto const fixture{make_fixture()};
    auto geometry{make_simulation_data(fixture)};
    geometry.player = fixture.player;
    auto built{sim::make_level_simulation_init_data(*definition, std::move(geometry))};
    ASSERT_TRUE(built) << built.error().front();
    ASSERT_TRUE(built->player);
    EXPECT_EQ(built->player->transform.location, (Vector3d{1200.0, 200.0, 0.0}));
    EXPECT_EQ(built->player->team, Team::Blue);
    EXPECT_EQ(built->player->health.health, Health{275});
    EXPECT_EQ(built->player->left_socket, fixture.player.left_socket);
    EXPECT_EQ(built->player->right_socket, fixture.player.right_socket);
    EXPECT_EQ(built->player->body_transform, fixture.player.body_transform);
    EXPECT_EQ(built->fighters.speed, 1234.f);
    EXPECT_EQ(built->clock_settings.tick_rate.hz(), 30.0);
    EXPECT_EQ(built->level_metadata.id.value, "headless-player");

    LevelSim simulation{std::move(*built)};
    simulation.finish_initialisation();
    auto const* player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    auto commands{simulation.player_ship_commands()};
    ASSERT_TRUE(commands);
    commands->set_forward_input(1.f);
    simulation.start();
    simulation.advance(0.5);
    EXPECT_EQ(simulation.clock().get_completed_ticks().value(), 15);
    EXPECT_GT(player->physical_state().transform.location.x(), 1200.0);
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 1);
}

TEST(NativeSimulation, NativeStartupRejectsInvalidSpawnGeometryBeforeConstructingWorld) {
    auto data{make_simulation_data(make_fixture())};
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location =
        Vector3d{data.collision.entity_bounds.centre(EntityType::CapitalShip)};
    auto invalid{validate_level_simulation_init_data(data)};
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](auto const& error) {
        return error.contains("intersects the capital collision");
    }));

    data = make_simulation_data(make_fixture());
    data.capital_ships.fighter_spawn_slots_relative_transforms[1] =
        data.capital_ships.fighter_spawn_slots_relative_transforms[0];
    invalid = validate_level_simulation_init_data(data);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](auto const& error) {
        return error.contains("fighter spawn slots 0 and 1");
    }));
}

TEST(NativeSimulation, NativeStartupRejectsFutureSpawnsOutsideGrid) {
    auto definition{levels::authoring::DefinitionReader{}.read_level_source(R"(
      (level :id 'geometry :title "Geometry"
        :camera (camera :look-at '(capital) :offset-direction '(-1 0 0) :distance 1000)
        :entities (list (entity :id 'capital :archetype 'capital-ship :team 'blue
                               :position '(2000000 0 0) :spawn-at 5))))")};
    ASSERT_TRUE(definition);
    auto input{make_simulation_data(make_fixture())};
    auto built{sim::make_level_simulation_init_data(*definition, std::move(input))};
    ASSERT_FALSE(built);
    EXPECT_TRUE(std::ranges::any_of(built.error(), [](auto const& error) {
        return error.contains("outside the collision grid");
    }));
}

TEST(NativeSimulation, ScheduledSpawnQueryableInThinkingSameTick) {
    auto data{make_scheduled_battle()};
    data.tick_memory_capacity_bytes = 16 * 1024 * 1024;
    data.phase_memory_capacity_bytes = 16 * 1024 * 1024;
    data.turrets.target_refresh_frequency = ::ioj::PositiveFinite<float>::make(10.f);
    // Keep the muzzle outside the turret's collision bounds so it cannot block its own query.
    data.turrets.fire_point_offset = Vector3f{20.f, 0.f, 0.f};
    auto& initial_storage{data.level_events.schedule.turret_spawns};
    data.level_events.schedule.add_initial_spawn_group(EntityType::Turret, 0, 1);
    initial_storage.add_uninitialised(1);
    auto const initial{initial_storage.view()};
    initial.entity_indices()[0] = data.level_events.initialisation.entity_count++;
    set_vector(initial.view_locations(), 0, Vector3f{-1000.f, 1000.f, 0.f});
    set_rotation(initial.view_rotations(), 0, {});
    initial.teams()[0] = Team::Blue;
    initial.healths()[0] = Health{100};
    initial.laser_damages()[0] = 0;
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    auto const first_tick_frame_memory{simulation.phase_memory_stats()};
    EXPECT_EQ(first_tick_frame_memory.capacity_bytes, std::size_t{16 * 1024 * 1024})
        << "Frame memory uses the configured per-simulation capacity";
    EXPECT_EQ(first_tick_frame_memory.current_claimed_bytes, std::size_t{0})
        << "Frame memory is reset after the completed tick";
    EXPECT_TRUE(first_tick_frame_memory.last_frame_root_claim_count > 0)
        << "Turret scratch claimed frame memory during the tick";
    EXPECT_EQ(first_tick_frame_memory.outstanding_allocation_count, std::uint64_t{0})
        << "No turret scratch allocation survives the reset";
    simulation.advance(dt);
    EXPECT_TRUE(!simulation.turrets().get_entities().target_ids()[0].is_valid())
        << "Turret has no enemy before the scheduled spawn";
    simulation.advance(dt);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    simulation.advance(dt);
    EXPECT_EQ(simulation.turrets().get_entities().target_ids()[0],
              simulation.capital_ships().get_entities().entity_ids()[1])
        << "Thinking acquires the entity created in Preparation";
    simulation.advance(dt);
    EXPECT_TRUE(simulation.turrets().get_entities().target_ids()[0] ==
                simulation.capital_ships().get_entities().entity_ids()[1])
        << "Thinking retains the spawned enemy on the following tick";
}

TEST(NativeSimulation, LevelSimReturnsAllocationsToItsUpstream) {
    constexpr std::size_t frame_capacity{1024 * 1024};
    GameMemory upstream{*std::pmr::new_delete_resource()};
    auto data{make_battle()};
    data.tick_memory_capacity_bytes = frame_capacity;
    data.phase_memory_capacity_bytes = frame_capacity;

    {
        LevelSim simulation{std::move(data), upstream};
        auto const stats{simulation.memory_stats()};
        EXPECT_GE(stats.live_allocation_count, 2u);
        EXPECT_GE(stats.live_bytes, 2 * frame_capacity);
        EXPECT_EQ(stats.live_bytes, upstream.stats().live_bytes);
        EXPECT_EQ(simulation.phase_memory_stats().capacity_bytes, frame_capacity);
        EXPECT_EQ(simulation.tick_memory_stats().capacity_bytes, frame_capacity);
    }

    EXPECT_EQ(upstream.stats().live_allocation_count, 0u);
    EXPECT_EQ(upstream.stats().live_bytes, 0u);
}

TEST(NativeSimulation, PositionConditionsUseLiveSpatialDataAndRemainTerminal) {
    auto data{make_battle()};
    levels::ObjectiveDefinition objective{.team = Team::Green};
    auto const target{FiniteVector<double, 3>::make(Vector3d{-1000.0, 0.0, 0.0})};
    objective.expressions = {
        {ioj::levels::EntityReachedPositionExpr{.entity = mission_entity(0),
                                                .position = target,
                                                .radius =
                                                    ioj::NonNegativeFinite<double>::make(1.0)}},
        {ioj::levels::EntityReachedPositionExpr{.entity = mission_entity(1),
                                                .position = target,
                                                .radius =
                                                    ioj::NonNegativeFinite<double>::make(1.0)}},
        {ioj::levels::TimeElapsedExpr{.seconds = ioj::NonNegativeFinite<double>::make(0.1)}},
        {ioj::levels::AllExpr{.children = {0, 2}}}};
    objective.roots = ioj::levels::ObjectiveRoots{
        std::array<ioj::levels::ExprIndex, 3>{3, 1, levels::absent_expression}};
    install_mission(data, std::move(objective));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    EXPECT_FALSE(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()));
    simulation.start();
    simulation.advance(2 * simulation.clock().tick_period());

    DirectDamageEvents damage;
    damage.add(simulation.capital_ships().get_entities().entity_ids()[0], 100, {});
    damage.add(simulation.capital_ships().get_entities().entity_ids()[1], 100, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.advance(0.2);
    EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()),
              ObjectiveOutcome::Passed);
    EXPECT_EQ(simulation.mission().completed_objectives().size(), 1);
}

TEST(NativeSimulation, SchemeFutureEntityAndLateObserverProgressThroughLevelSimulation) {
    auto const authored{levels::authoring::DefinitionReader{}.read_level_source(R"(
      (level :id 'future-objective :title "Future objective"
        :camera (camera :look-at '(hero) :offset-direction '(-1 0 0) :distance 1000)
        :entities (list
          (entity :id 'hero :archetype 'capital-ship :team 'green :position '(-1000 0 0))
          (entity :id 'enemy :archetype 'capital-ship :team 'white :position '(1000 0 0) :spawn-at 0.2))
        :mission (mission :objectives (list
          (objective :id 'destroy :team 'green :pass (entity-destroyed 'enemy))
          (objective :id 'observe :team 'green :wins-level 'yes
            :depends-on (list (objective-resolved 'destroy 'passed))
            :pass (all-of (entity-destroyed 'enemy) (objective-resolved 'destroy 'passed)))))))")};
    ASSERT_TRUE(authored);
    auto data{make_battle()};
    data.clock_settings.tick_rate = Frequency::make(10.0);
    data.capital_ships.max_health = ::ioj::settings::PositiveHealth::make(Health{100});
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{levels::compile_level(*authored, clock, data.capital_ships, data.turrets)};
    ASSERT_TRUE(compiled);
    data.level_events = std::move(compiled->events);
    data.mission = std::move(compiled->mission);
    data.expressions = std::move(compiled->expressions);
    data.level_metadata = std::move(compiled->level_metadata);
    LevelSim simulation{std::move(data)};
    auto const& metadata{simulation.level_metadata()};
    EXPECT_EQ(metadata.id.value, "future-objective");
    EXPECT_EQ(metadata.title, "Future objective");
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(0.1);
    EXPECT_FALSE(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()));
    simulation.advance(0.1);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    simulation.advance(0.1);
    ASSERT_EQ(simulation.capital_ships().num_instances(), 2);
    kill_enemy(simulation);
    simulation.advance(0.1);
    EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{0}, simulation.expressions()),
              ObjectiveOutcome::Passed);
    EXPECT_FALSE(simulation.mission().outcome(ioj::sim::ObjectiveId{1}, simulation.expressions()));
    simulation.advance(0.1);
    EXPECT_EQ(simulation.mission().outcome(ioj::sim::ObjectiveId{1}, simulation.expressions()),
              ObjectiveOutcome::Passed);
    EXPECT_FALSE(simulation.player_team_completion());
}

TEST(NativeSimulation, LevelCompletionUsesPlayerTeamAndDoesNotCancelOptionalObjectives) {
    using namespace levels;
    auto const timed_objective{[](std::string name, Team const team, double const seconds) {
        ObjectiveDefinition objective{.id = ObjectiveName{std::move(name)}, .team = team};
        objective.expressions = {
            {TimeElapsedExpr{.seconds = NonNegativeFinite<double>::make(seconds)}}};
        objective.roots[ObjectiveOutcome::Passed] = 0;
        return objective;
    }};

    auto data{make_battle()};
    data.clock_settings.tick_rate = Frequency::make(10.0);
    add_player_spawn(data, {});
    data.player->team = Team::Green;
    data.player->transform.location = {0.0, 2000.0, 0.0};
    MissionDefinition definition{.objectives = {timed_objective("enemy", Team::White, 0.2),
                                                timed_objective("player", Team::Green, 0.3),
                                                timed_objective("optional", Team::Green, 10.0)}};
    definition.objectives[0].wins_level = ObjectiveWinsLevel::Yes;
    definition.objectives[1].wins_level = ObjectiveWinsLevel::Yes;
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{Mission::compile(definition, {}, clock, data.expressions)};
    ASSERT_TRUE(compiled);
    data.mission = std::move(*compiled);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    EXPECT_FALSE(simulation.player_team_completion());
    simulation.start();
    simulation.advance(0.4);
    auto const result{simulation.player_team_completion()};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->team, Team::Green);
    EXPECT_EQ(result->outcome, MissionOutcome::Defeat);
    EXPECT_EQ(result->triggering_objective_outcome, ObjectiveOutcome::Passed);
    EXPECT_EQ(simulation.mission().objectives().name(result->triggering_objective).value, "enemy");
    EXPECT_EQ(result->tick.value(), 3);
    EXPECT_EQ(simulation.mission().active_objectives().size(), 2);
    EXPECT_TRUE(simulation.player_team_completion());
    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), result->tick);
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::MissionCompleted);
    EXPECT_EQ(record->completion.completed_ticks, result->tick);
    ASSERT_TRUE(simulation.player_team_completion());
    EXPECT_EQ(simulation.player_team_completion()->outcome, MissionOutcome::Defeat);
}

TEST(NativeSimulation, QuantizedDurationPrecedesPauseAndDiscardsPendingSpawns) {
    auto data{make_scheduled_battle()};
    SimClock clock;
    clock.initialise(data.clock_settings);
    ASSERT_TRUE(data.level_events.schedule.add_level_duration_limit(
        data.expressions, clock, Duration::make(0.21)));
    std::array const pause{EventHandle{0, EventKind::Pause}};
    ASSERT_TRUE(data.level_events.schedule.add_timed_trigger(
        data.expressions, clock, Duration::make(0.21), pause));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(0.2);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{2});

    simulation.advance(0.225);
    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_TRUE(simulation.mission().completions().empty());
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::DurationReached);
    EXPECT_EQ(record->completion.completed_ticks, SimTick{3});
    EXPECT_EQ(record->final_state.completed_tick, SimTick{3});
    EXPECT_FALSE(simulation.take_finalized_telemetry_run());

    simulation.advance(1.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    simulation.start();
    simulation.advance(0.2);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{6});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_NEAR(simulation.read_access().interpolation_alpha(), 0.25, 1.e-9);
}

TEST(NativeSimulation, PlayerlessMissionCompletionPrecedesDurationPauseAndSpawns) {
    auto data{make_scheduled_battle()};
    levels::ObjectiveDefinition objective{.id = levels::ObjectiveName{"win"},
                                          .team = Team::Blue,
                                          .wins_level = ObjectiveWinsLevel::Yes};
    objective.expressions = {{levels::TimeElapsedExpr{NonNegativeFinite<double>::make(0.2)}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto mission{Mission::compile({.objectives = {objective}}, {}, clock, data.expressions)};
    ASSERT_TRUE(mission);
    data.mission = std::move(*mission);
    ASSERT_TRUE(data.level_events.schedule.add_level_duration_limit(
        data.expressions, clock, Duration::make(0.21)));
    std::array const pause{EventHandle{0, EventKind::Pause}};
    ASSERT_TRUE(data.level_events.schedule.add_timed_trigger(
        data.expressions, clock, Duration::make(0.21), pause));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);

    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_FALSE(simulation.player_team_completion());
    auto const completion{simulation.mission().team_completion(Team::Blue)};
    ASSERT_TRUE(completion);
    EXPECT_EQ(completion->outcome, MissionOutcome::Victory);
    EXPECT_EQ(completion->tick, SimTick{3});
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::MissionCompleted);
    EXPECT_EQ(record->completion.completed_ticks, completion->tick);
    simulation.advance(1.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
}

TEST(NativeSimulation, DurationEventsDoNotRequireTelemetryRecording) {
    auto data{make_battle()};
    data.telemetry_recording.enabled = false;
    SimClock clock;
    clock.initialise(data.clock_settings);
    ASSERT_TRUE(data.level_events.schedule.add_level_duration_limit(
        data.expressions, clock, Duration::make(0)));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{1});
}

TEST(NativeSimulation, RunsWithoutACompletionOrDurationEvent) {
    auto data{make_battle()};
    data.clock_settings.tick_rate = Frequency::make(10);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{10});
}

TEST(NativeSimulation, QueuedPausePreservesPendingSpawnsAndTelemetryUntilResume) {
    auto data{make_scheduled_battle()};
    data.telemetry_recording.interval_ticks = 1;
    SimClock clock;
    clock.initialise(data.clock_settings);
    std::array const pause{EventHandle{0, EventKind::Pause}};
    ASSERT_TRUE(data.level_events.schedule.add_timed_trigger(
        data.expressions, clock, Duration::make(0.21), pause));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(0.425);

    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().phase(), SimulationPhase::BetweenTicks);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_EQ(simulation.level_telemetry_manager().state(),
              LevelTelemetryManager::State::Recording);
    EXPECT_FALSE(simulation.take_finalized_telemetry_run());

    simulation.pause();
    simulation.pause();
    simulation.advance(1.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    simulation.start();
    simulation.advance(clock.tick_period());
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{5});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 2);
    EXPECT_NEAR(simulation.read_access().interpolation_alpha(), 0.25, 1.e-9);
    EXPECT_EQ(simulation.level_telemetry_manager().state(),
              LevelTelemetryManager::State::Recording);

    simulation.pause();
    EXPECT_EQ(simulation.state(), LevelState::Running);
    simulation.advance(0.0);
    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{5});
    EXPECT_FALSE(simulation.take_finalized_telemetry_run());

    simulation.finalize_telemetry_run(telemetry::RunEndReason::WorldEnd);
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::WorldEnd);
    EXPECT_EQ(record->completion.completed_ticks, SimTick{5});
    ASSERT_EQ(record->battle_samples.size(), 6);
    EXPECT_EQ(record->battle_samples.back().completed_tick, SimTick{5});
}

TEST(NativeSimulation, LaserFrameOutputsTest) {
    auto data{make_battle()};
    data.clock_settings.tick_rate = ioj::Frequency::make(10.0);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto queue_shot = [](LevelSim& level, float const location) {
        lasers::LaserSpawnRequests requests;
        tests::add_laser_spawn(requests,
                               {{location, 0.f, 0.f}},
                               {},
                               {},
                               1,
                               2000.f,
                               10000.f,
                               level.capital_ships().get_entities().entity_ids()[0],
                               {Team::Green, EntityType::Fighter});
        LevelSimTestAccess::queue_laser_spawns(level, requests.const_view());
    };
    queue_shot(simulation, 700.f);
    queue_shot(simulation, 500.f);
    simulation.start();
    simulation.advance(0.425);
    auto const frame{simulation.read_access()};
    auto const& lasers{frame.lasers()};
    auto const hit_ticks{lasers.hit_ticks()};
    EXPECT_EQ(lasers.num_instances(), 0) << "Impacted lasers leave authoritative storage";
    EXPECT_EQ(lasers.hits().num(), 2) << "Both impacts survive the final empty fixed tick";
    EXPECT_EQ(hit_ticks.size(), std::size_t{2}) << "Impact tick indices remain aligned";
    if (hit_ticks.size() == 2) {
        EXPECT_EQ(hit_ticks[0].value(), std::uint64_t{1})
            << "First impact keeps its deterministic tick";
        EXPECT_EQ(hit_ticks[1].value(), std::uint64_t{2})
            << "Second impact keeps its deterministic tick";
        EXPECT_TRUE((lasers.hits().sources()[0] == LaserSource{Team::Green, EntityType::Fighter}))
            << "Neutral source is retained after removal";
    }
    simulation.advance(0.0);
    EXPECT_EQ(simulation.read_access().lasers().hits().num(), 0)
        << "Next frame does not repeat consumed impacts";
}

TEST(NativeSimulation, LevelSimCollisionBoundsTest) {
    auto data{make_battle()};
    std::array<collision::WorldAABB, 1> static_bounds{
        collision::WorldAABB{{{-10.f, 490.f, -10.f}}, {{10.f, 510.f, 10.f}}}};
    LevelSim simulation{std::move(data)};
    simulation.set_static_collision(static_bounds);
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};

    auto const& queries{simulation.spatial_query_manager()};
    auto const initial_bounds{queries.entity_collision_bounds()};
    auto const initial_ids{initial_bounds.entity_ids()};
    EXPECT_NE(
        std::ranges::find(initial_ids, simulation.capital_ships().get_entities().entity_ids()[0]),
        initial_ids.end());
    EXPECT_EQ(queries.static_collision_bounds().num(), 1);

    simulation.start();
    simulation.advance(clk_period);
    auto const current_static_bounds{queries.static_collision_bounds()};
    ASSERT_EQ(current_static_bounds.num(), 1);
    EXPECT_EQ(current_static_bounds.view_min_points()[0].y(), 490.f);
    EXPECT_EQ(current_static_bounds.view_max_points()[0].y(), 510.f);
}

TEST(NativeSimulation, LevelSimCompiledInitialisationTest) {
    auto data{make_battle()};
    add_player_spawn(data, {});
    add_turret_spawn(data, {{0.f, -1000.f, 0.f}}, {0.f, 90.f, 0.f}, Team::Green, 20, 5);
    add_turret_spawn(data, {{0.f, 1000.f, 0.f}}, {}, Team::White, 30, 7);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const* player{simulation.player_ship_simulation()};
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 5)
        << "Every compiled initial entity is registered";
    auto const& health_table{simulation.entity_tables().health};
    EXPECT_EQ(health_table.health(player->unique_entity_id), player->health().health)
        << "Player health is stored in the world health table";
    auto const& turret_view{simulation.turrets()};
    auto const turrets{turret_view.get_entities()};
    auto const turret_count{turrets.num()};
    auto const turret_teams{turrets.teams()};
    auto const turret_ids{turrets.entity_ids()};
    auto const turret_healths{health_table.const_view<EntityType::Turret>()};
    for (std::uint32_t i{}; i < turret_count; ++i) {
        auto const rotated{turret_teams[i] == Team::Green};
        EXPECT_EQ(turret_healths.health(turret_ids[i]), turret_view.healths().health(turret_ids[i]))
            << "Turret health is allocated in the world health table";
        EXPECT_EQ(turret_view.healths().health(turret_ids[i]).value(), rotated ? 20 : 30)
            << "Compiled turret health is retained";
        EXPECT_EQ(turrets.view_rotations().yaws()[i], rotated ? 90.f : 0.f)
            << "Compiled turret rotation is retained";
    }
    EXPECT_EQ(turrets.num(), 2) << "Both compiled turrets are registered";
}

TEST(NativeSimulation, PlayerPreservesPartialSpawnHealth) {
    auto data{make_battle()};
    add_player_spawn(data, {});
    data.player->health = {Health{50}, Health{100}};

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();

    auto const* const player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->health().health.value(), 50);
    EXPECT_EQ(player->health().max_health.value(), 100);
}

TEST(NativeSimulation, MixedWorldRemovalAndSubsequentSpawnPreserveHealthMappings) {
    auto data{make_battle()};
    player::PlayerSpawnData player_spawn{};
    player_spawn.health = {Health{100}, Health{100}};
    player_spawn.transform.location = {0.0, 5000.0, 0.0};
    add_player_spawn(data, player_spawn);
    add_turret_spawn(data, {{0.f, 2000.f, 0.f}}, {}, Team::Green, 100, 0);
    add_turret_spawn(data, {{0.f, 3000.f, 0.f}}, {}, Team::White, 100, 0);
    add_turret_spawn(data, {{0.f, 4000.f, 0.f}}, {}, Team::Green, 100, 0);

    data.capital_ships.fighter_spawn_slots = 2;
    data.capital_ships.fighter_spawn_slots_relative_transforms.resize(2);
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.y() = -100.0;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.y() = 100.0;
    data.fighters.health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.fighters.speed = 0.f;
    auto const capital_events{data.level_events.schedule.capital_spawns.view()};
    std::ranges::fill(capital_events.initial_fighter_spawn_delays(), 0.f);

    auto& schedule{data.level_events.schedule};
    auto const payload_offset{schedule.turret_spawns.num()};
    schedule.turret_spawns.add_defaulted(1);
    auto const scheduled_turret{schedule.turret_spawns.view(payload_offset, 1)};
    scheduled_turret.entity_indices()[0] = data.level_events.initialisation.entity_count++;
    set_vector(scheduled_turret.view_locations(), 0, {{0.f, 6000.f, 0.f}});
    scheduled_turret.teams()[0] = Team::White;
    scheduled_turret.healths()[0] = Health{75};
    add_timed_spawn(data, EntityType::Turret, payload_offset, 1, SimTick{4});

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto const tick_period{simulation.clock().tick_period()};
    simulation.advance(tick_period);
    simulation.advance(tick_period);

    auto const* const player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr);
    auto const surviving_capital{simulation.capital_ships().get_entities().entity_ids()[1]};
    std::int32_t fighter_count_before{};
    std::array<EntityId, 6> victims{};
    {
        auto const before{simulation.read_access()};
        auto const fighters{before.fighters().entities()};
        auto const turrets{before.turrets().get_entities()};
        ASSERT_GE(fighters.num(), 4);
        ASSERT_EQ(turrets.num(), 3);
        fighter_count_before = fighters.num();
        victims = {
            simulation.capital_ships().get_entities().entity_ids()[0],
            fighters.entity_ids()[0],
            fighters.entity_ids()[fighters.num() - 1],
            turrets.entity_ids()[0],
            turrets.entity_ids()[turrets.num() - 1],
            player->unique_entity_id,
        };
    }
    DirectDamageEvents damage;
    for (auto const victim : victims) {
        damage.add(victim, 10000, surviving_capital);
    }
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());

    simulation.advance(tick_period);
    expect_health_mappings(simulation);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_EQ(simulation.fighters().num_instances(), fighter_count_before - 2);
    EXPECT_EQ(simulation.turrets().num_instances(), 1);
    EXPECT_TRUE(is_dead(player->health().health));
    expect_entity_unregistered(simulation, player->unique_entity_id);

    simulation.advance(tick_period);
    EXPECT_EQ(simulation.turrets().num_instances(), 1);
    simulation.advance(tick_period);
    expect_health_mappings(simulation);
    EXPECT_EQ(simulation.turrets().num_instances(), 2);
}

TEST(NativeSimulation, LevelSimReconstructionTest) {
    std::optional<LevelSim> simulation;
    EXPECT_FALSE(simulation.has_value()) << "Construction can be delayed";
    auto first_data{make_battle()};
    add_mission(first_data);
    simulation.emplace(std::move(first_data));
    simulation->finish_initialisation();
    simulation->start();
    auto const tick_period{simulation->clock().tick_period()};
    simulation->advance(tick_period);
    kill_enemy(*simulation);
    simulation->advance(tick_period);
    simulation.reset();
    auto second_data{make_battle()};
    add_mission(second_data);
    simulation.emplace(std::move(second_data));
    simulation->finish_initialisation();
    EXPECT_EQ(simulation->clock().get_completed_ticks().value(), std::uint64_t{0})
        << "Fresh clock starts at zero";
    EXPECT_EQ(simulation->entity_ledger().count_alive(), 2)
        << "Fresh ledger contains both entities";
    EXPECT_EQ(simulation->entity_ledger().num_unique_ids_issued(), 2)
        << "Fresh ledger has no prior history";
    EXPECT_FALSE(simulation->player_team_completion().has_value())
        << "No pending result survives reconstruction";
}

TEST(NativeSimulation, LevelSimPlanarMovementOffsetTest) {
    auto data{make_battle()};
    add_player_spawn(data, {});
    data.player->flight_models.initial_slot = player::FlightModelSlot::Fourth;
    auto& gunship{
        player::flight_model_profile(data.player->flight_models, player::FlightModelSlot::Fourth)};
    gunship.config.translation.right.manual.response.mode = player::ResponseMode::Direct;
    gunship.config.translation.up.manual.response.mode = player::ResponseMode::Direct;

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto* const player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr) << "Planar movement fixture has a player";

    auto const dt{simulation.clock().tick_period()};
    auto local_velocity = [player] {
        return player->physical_state().transform.inverse_transform_vector_no_scale(
            player->physical_state().velocity);
    };

    simulation.player_ship_commands()->set_right_input(1.f);
    simulation.advance(dt);
    EXPECT_TRUE((std::abs(local_velocity().y() - 3000.0) <= 0.1))
        << "Held lateral input adds the configured local offset";
    auto const initial_target_scale{player->sampled_target_speed_scale()};
    EXPECT_TRUE((std::abs(initial_target_scale.x()) <= 1.e-4 &&
                 std::abs(initial_target_scale.y()) <= 1.e-4))
        << "Lateral input does not change desired planar velocity";

    simulation.player_ship_commands()->set_right_input(0.f);
    simulation.player_ship_commands()->set_up_input(1.f);
    simulation.advance(dt);
    EXPECT_TRUE((std::abs(local_velocity().z() - 3000.0) <= 0.1))
        << "Held vertical input adds the configured local offset";
    EXPECT_TRUE((std::abs(local_velocity().y()) <= 0.1))
        << "Released lateral input removes its local offset";

    simulation.player_ship_commands()->set_up_input(0.f);
    simulation.advance(dt);
    auto const released_velocity{local_velocity()};
    EXPECT_TRUE((std::abs(released_velocity.z()) <= 0.1))
        << "Released vertical input removes its local offset";
    auto const final_target_scale{player->sampled_target_speed_scale()};
    EXPECT_TRUE(
        (std::abs(final_target_scale.x()) <= 1.e-4 && std::abs(final_target_scale.y()) <= 1.e-4))
        << "Movement offsets remain temporary";
}

TEST(NativeSimulation, LevelSimOverlapResponseTest) {
    LevelSim simulation{make_overlap_response_battle()};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    auto* player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr) << "The overlap fixture has a movable low-health entity";

    simulation.player_ship_commands()->set_right_input(1.f);
    auto const player_id{player->unique_entity_id};
    auto const capital{simulation.capital_ships().get_entities().entity_ids()[0]};
    auto const& ledger{simulation.entity_ledger()};

    for (std::int32_t overlap_detection{}; overlap_detection < 3; ++overlap_detection) {
        simulation.advance(dt);
        // advance publishes a fresh overlap batch each tick.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const events{simulation.spatial_query_manager().aabb_overlap_events()};
        EXPECT_EQ(events.entity_entity_overlaps.num(), 1)
            << "The tick captures one unique dynamic overlap";
        EXPECT_EQ(events.entity_static_overlaps.num(), 0) << "The tick captures no static overlap";

        EXPECT_EQ(observe_entity(simulation, capital)->health.value(),
                  5000 - (overlap_detection + 1) * 50)
            << "The high-health capital receives damage in the detection tick";
        if (overlap_detection < 2) {
            EXPECT_EQ(observe_entity(simulation, player_id)->health.value(),
                      150 - (overlap_detection + 1) * 50)
                << "The low-health entity receives damage in the detection tick";
        }
    }

    EXPECT_FALSE(entity_is_alive(simulation, player_id))
        << "The low-health entity dies after three detected overlaps";
    EXPECT_EQ(observe_entity(simulation, capital)->health.value(), 4850)
        << "The capital receives one contribution per detected tick";
    EXPECT_TRUE(ledger.unique_entities().life_state()[ledger.history_index(player_id)] ==
                LifeState::Unknown)
        << "Overlap death uses the environmental death path";
    EXPECT_EQ(ledger.count_kills(), 0) << "Environmental overlap death gives no combat kill";
}

TEST(NativeSimulation, WorldlessLevelSimulationTest) {
    auto first_data{make_battle()};
    auto second_data{make_battle()};
    add_mission(first_data);
    add_mission(second_data);
    LevelSim first{std::move(first_data)};
    LevelSim second{std::move(second_data)};
    EXPECT_EQ(first.state(), LevelState::Uninitialised)
        << "Construction leaves external setup open";
    first.advance(1.0);
    EXPECT_EQ(first.clock().get_completed_ticks().value(), std::uint64_t{0})
        << "Uninitialised simulation ignores elapsed time";
    first.finish_initialisation();
    second.finish_initialisation();
    EXPECT_EQ(first.state(), LevelState::Paused)
        << "Finishing initialization pauses the simulation";
    EXPECT_EQ(first.level_telemetry_manager().state(), LevelTelemetryManager::State::Recording)
        << "Default metadata permits telemetry recording";
    first.pause();
    first.pause();
    first.advance(0.0);
    EXPECT_TRUE(first.player_ship_simulation() == nullptr) << "No player is needed";
    EXPECT_EQ(first.entity_ledger().count_alive(), 2) << "Both capitals are registered";
    first.start();
    second.start();
    auto const dt{first.clock().tick_period()};
    first.advance(dt);
    EXPECT_EQ(second.clock().get_completed_ticks().value(), std::uint64_t{0})
        << "Independent clock remains at zero";
    kill_enemy(first);
    first.advance(dt);
    second.advance(dt);
    EXPECT_EQ(first.entity_ledger().count_alive(), 1)
        << "Death is effective before physical removal";
    first.advance(dt);
    EXPECT_EQ(first.capital_ships().num_instances(), 1)
        << "Damage removes only the first battle's enemy";
    EXPECT_EQ(second.capital_ships().num_instances(), 2) << "Other battle is unaffected";
    auto result{first.player_team_completion()};
    EXPECT_FALSE(result.has_value()) << "Playerless simulation does not choose a player's outcome";
    EXPECT_EQ(first.mission().outcome(ioj::sim::ObjectiveId{0}, first.expressions()),
              ObjectiveOutcome::Passed);
    EXPECT_FALSE(first.player_team_completion().has_value())
        << "Playerless queries leave team results available on the mission";
    EXPECT_FALSE(first.mission().completions().empty());
    first.pause();
    auto const paused_ticks{first.clock().get_completed_ticks()};
    first.advance(dt);
    EXPECT_EQ(first.clock().get_completed_ticks(), paused_ticks)
        << "Paused battle does not advance";
    first.start();
    first.advance(dt);
    EXPECT_EQ(first.clock().get_completed_ticks(), SimTick{paused_ticks.value() + 1})
        << "Battle resumes";
}

TEST(NativeSimulation, LevelTelemetryRunRecordTest) {

    auto data{make_battle()};
    LevelSim simulation{std::move(data)};
    simulation.set_telemetry_metadata(telemetry::RunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryTest",
        .level_id = "telemetry-test",
        .level_display_name = "Telemetry Test",
        .launched_utc = "2026-09-06T12:00:00Z",
    });
    EXPECT_EQ(simulation.level_telemetry_manager().state(),
              LevelTelemetryManager::State::Uninitialised)
        << "Metadata configuration waits for initialisation";
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.level_telemetry_manager().state(), LevelTelemetryManager::State::Recording)
        << "Finishing starts telemetry";
    EXPECT_EQ(simulation.state(), LevelState::Paused) << "Finishing leaves the simulation paused";
    simulation.start();

    auto const& telemetry{simulation.level_telemetry_manager()};
    EXPECT_EQ(telemetry.state(), LevelTelemetryManager::State::Recording)
        << "Simulation initialization starts telemetry recording";

    simulation.advance(1.0);
    auto const& clock{simulation.clock()};
    simulation.advance(4.0 * clock.tick_period());
    simulation.finalize_telemetry_run(telemetry::RunEndReason::WorldEnd);
    EXPECT_EQ(simulation.state(), LevelState::Running)
        << "Interrupted telemetry finalization does not pause simulation";

    auto record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record.has_value()) << "Finalized manager yields one run record";
    EXPECT_FALSE(simulation.take_finalized_telemetry_run().has_value())
        << "A finalized run record is yielded only once";
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::WorldEnd)
        << "Completion preserves its end reason";
    EXPECT_EQ(record->completion.completed_ticks, clock.get_completed_ticks())
        << "Completion preserves completed ticks";
    auto const& battle{record->battle_samples};
    ASSERT_GE(battle.size(), 3);
    EXPECT_EQ(battle.front().completed_tick, SimTick{0});
    EXPECT_EQ(battle.back().completed_tick, clock.get_completed_ticks());
    EXPECT_DOUBLE_EQ(static_cast<double>(battle.back().completed_tick.value()) /
                         record->tick_rate_hz.hz(),
                     clock.simulation_time());
    EXPECT_DOUBLE_EQ(record->tick_rate_hz.hz(), clock.tick_rate().hz());

    EXPECT_EQ(record->final_state.completed_tick, clock.get_completed_ticks());
    EXPECT_EQ(telemetry::total_count(record->final_state.alive),
              simulation.entity_ledger().count_alive());
}

} // namespace tests
