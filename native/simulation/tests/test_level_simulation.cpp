#include "support/mission_test_support.h"
#include "support/simulation_checks.h"
#include <ioj/column_math.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/collision/world_aabbs.h>
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
auto make_battle_config() -> SimulationScenario {
    SimulationScenario data;
    data.runtime.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(16);
    data.runtime.capital_ships.fighter_spawn_slots = 0;
    for (auto const type : EnumTraits<EntityType>::values) {
        data.runtime.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}

auto make_battle() -> SimulationScenario {
    auto data{make_battle_config()};
    add_capital_spawn(data,
                      {{-1000.f, 0.f, 0.f}},
                      Team::Green,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    add_capital_spawn(data,
                      {{1000.f, 0.f, 0.f}},
                      Team::White,
                      60.f,
                      60.f,
                      ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    return data;
}

auto make_scheduled_battle() -> SimulationScenario {
    auto data{make_battle_config()};
    data.runtime.capital_ships.max_health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.runtime.clock_settings.tick_rate = Frequency::make(10.0);
    auto definition{levels::authoring::DefinitionReader{}.read_level_source(R"(
        (level :id 'scheduled-battle :title "Scheduled Battle"
          :camera (camera :look-at '(hero) :offset-direction '(-1 0 0) :distance 1000)
          :entities (list
            (entity :id 'hero :archetype 'capital-ship :team 'blue :position '(-1000 0 0))
            (entity :id 'enemy :archetype 'capital-ship :team 'red :position '(1000 0 0)
              :spawn-at 0.21))))")};
    assert(definition);
    data.entities.assign(definition->entities().begin(), definition->entities().end());
    return data;
}

auto make_overlap_response_battle() -> SimulationScenario {
    auto data{make_battle()};
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        ioj::Positive<std::int32_t>{50};
    for (auto& entity : data.entities) {
        entity.overrides.health = settings::PositiveHealth{Health{5000}};
    }
    add_player_spawn(data, {});
    data.runtime.player->transform.location = {-1000.0, 0.0, 0.0};
    data.runtime.player->health = {Health{150}, Health{150}};
    data.runtime.collision.entity_bounds.set_half_extents(EntityType::CapitalShip,
                                                          {{100.f, 100.f, 100.f}});
    return data;
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
                       .overrides = {.health = settings::PositiveHealth{Health{275}}}}},
         .settings = {.tick = {.tick_rate = Frequency::make(30.0)},
                      .fighters = {.speed = 1234.f}}})};
    ASSERT_TRUE(definition);
    auto const fixture{make_fixture()};
    LevelSimInitData geometry{make_simulation_data(fixture)};
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
    LevelSimInitData data{make_simulation_data(make_fixture())};
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
    LevelSimInitData input{make_simulation_data(make_fixture())};
    auto built{sim::make_level_simulation_init_data(*definition, std::move(input))};
    ASSERT_FALSE(built);
    EXPECT_TRUE(std::ranges::any_of(built.error(), [](auto const& error) {
        return error.contains("outside the collision grid");
    }));
}

TEST(NativeSimulation, ScheduledSpawnQueryableInThinkingSameTick) {
    auto data{make_scheduled_battle()};
    data.runtime.tick_memory_capacity_bytes = 16 * 1024 * 1024;
    data.runtime.phase_memory_capacity_bytes = 16 * 1024 * 1024;
    data.runtime.turrets.target_refresh_frequency = ::ioj::PositiveFinite<float>::make(10.f);
    // Keep the muzzle outside the turret's collision bounds so it cannot block its own query.
    data.runtime.turrets.fire_point_offset = Vector3f{20.f, 0.f, 0.f};
    add_turret_spawn(data,
                     {-1000.f, 1000.f, 0.f},
                     {},
                     Team::Blue,
                     settings::PositiveHealth{Health{100}},
                     NonNegative<std::int32_t>{0});
    LevelSim simulation{std::move(data).compile()};
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
    data.runtime.tick_memory_capacity_bytes = frame_capacity;
    data.runtime.phase_memory_capacity_bytes = frame_capacity;

    {
        LevelSim simulation{std::move(data).compile(), upstream};
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
    LevelSim simulation{std::move(data).compile()};
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
    auto data{make_battle_config().compile()};
    data.clock_settings.tick_rate = Frequency::make(10.0);
    data.capital_ships.max_health = ::ioj::settings::PositiveHealth::make(Health{100});
    auto compiled{levels::compile_level(*authored, data)};
    ASSERT_TRUE(compiled);
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
    data.runtime.clock_settings.tick_rate = Frequency::make(10.0);
    add_player_spawn(data, {});
    data.runtime.player->team = Team::Green;
    data.runtime.player->transform.location = {0.0, 2000.0, 0.0};
    MissionDefinition definition{.objectives = {timed_objective("enemy", Team::White, 0.2),
                                                timed_objective("player", Team::Green, 0.3),
                                                timed_objective("optional", Team::Green, 10.0)}};
    definition.objectives[0].wins_level = ObjectiveWinsLevel::Yes;
    definition.objectives[1].wins_level = ObjectiveWinsLevel::Yes;
    data.mission = std::move(definition);
    LevelSim simulation{std::move(data).compile()};
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
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), result->tick);
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::MissionCompleted);
    EXPECT_EQ(record->completion.completed_ticks, result->tick);
    ASSERT_TRUE(simulation.player_team_completion());
    EXPECT_EQ(simulation.player_team_completion()->outcome, MissionOutcome::Defeat);
}

TEST(NativeSimulation, PreparationEventsSpawnInFifoOrderWithoutReplayingConsumedEvents) {
    auto data{make_battle().compile()};
    auto& definitions{data.level_events};
    definitions.initial_events.clear();
    definitions.spawn_groups = {};
    definitions.add_spawn_group(EntityType::CapitalShip, 0, 1);
    definitions.add_spawn_group(EntityType::CapitalShip, 1, 1);
    auto const first{definitions.add_spawn_event({0, 1})};
    auto const second{definitions.add_spawn_event({1, 1})};
    std::array const events{second, first};
    add_timed_events(data, Duration::make(0), events);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto const period{simulation.clock().tick_period()};

    simulation.advance(period);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 0);
    EXPECT_TRUE(std::ranges::equal(simulation.events().pending_preparation_events(), events));
    simulation.advance(period);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 2);
    EXPECT_TRUE(std::ranges::equal(simulation.entity_ledger().unique_entities().teams(),
                                   std::array{Team::White, Team::Green}));
    EXPECT_TRUE(std::ranges::equal(simulation.events().preparation_events(), events));
    EXPECT_TRUE(simulation.events().pending_preparation_events().empty());
    simulation.advance(period);
    EXPECT_EQ(simulation.entity_ledger().num_unique_ids_issued(), 2);
}

TEST(NativeSimulation, QuantizedTerminalEventsDispatchInFifoOrderAndRetainPendingEvents) {
    auto data{make_scheduled_battle().compile()};
    auto& definitions{data.level_events};
    auto const second{definitions.add_stop_event(telemetry::RunEndReason::WorldEnd)};
    auto const first{definitions.add_stop_event(telemetry::RunEndReason::DurationReached)};
    EventHandle const pause{0, EventKind::Pause};
    std::array const events{pause, first, second, pause};
    add_timed_events(data, Duration::make(0.21), events);
    auto expected_history{definitions.initial_events};
    std::erase_if(expected_history,
                  [](EventHandle const event) { return !is_tick_end(event.kind()); });
    expected_history.append_range(events);
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(0.2);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{2});

    simulation.advance(0.225);
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_TRUE(simulation.mission().completions().empty());
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::DurationReached);
    EXPECT_EQ(record->completion.completed_ticks, SimTick{3});
    EXPECT_EQ(record->final_state.completed_tick, SimTick{3});
    EXPECT_FALSE(simulation.take_finalized_telemetry_run());
    EXPECT_TRUE(std::ranges::equal(simulation.events().tick_end_events(), expected_history));
    EXPECT_TRUE(std::ranges::equal(simulation.events().pending_tick_end_events(),
                                   std::array{second, pause}));
    ASSERT_EQ(simulation.events().pending_preparation_events().size(), 1);
    EXPECT_EQ(simulation.events().preparation_events().size(), 2);

    simulation.advance(1.0);
    simulation.advance(0.0);
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_TRUE(std::ranges::equal(simulation.events().tick_end_events(), expected_history));
    EXPECT_TRUE(std::ranges::equal(simulation.events().pending_tick_end_events(),
                                   std::array{second, pause}));
    EXPECT_EQ(simulation.events().pending_preparation_events().size(), 1);
    EXPECT_FALSE(simulation.take_finalized_telemetry_run());
}

TEST(NativeSimulation, PlayerlessMissionCompletionStopsDispatchAndRetainsLaterEvents) {
    auto data{make_scheduled_battle()};
    levels::ObjectiveDefinition objective{.id = levels::ObjectiveName{"win"},
                                          .team = Team::Blue,
                                          .wins_level = ObjectiveWinsLevel::Yes};
    objective.expressions = {{levels::TimeElapsedExpr{NonNegativeFinite<double>::make(0.2)}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    data.mission = levels::MissionDefinition{.objectives = {std::move(objective)}};
    auto compiled{std::move(data).compile()};
    add_duration_limit(compiled, Duration::make(0.21));
    std::array const pause{EventHandle{0, EventKind::Pause}};
    add_timed_events(compiled, Duration::make(0.21), pause);
    LevelSim simulation{std::move(compiled)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);

    EXPECT_EQ(simulation.state(), LevelState::Stopped);
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
    EXPECT_EQ(simulation.events().tick_end_events().size(), 7);
    EXPECT_EQ(simulation.events().pending_tick_end_events().size(), 2);
    EXPECT_EQ(simulation.events().pending_preparation_events().size(), 1);
    simulation.advance(1.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{3});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
}

TEST(NativeSimulation, DurationEventsDoNotRequireTelemetryRecording) {
    auto data{make_battle().compile()};
    data.telemetry_recording.enabled = false;
    add_duration_limit(data, Duration::make(0));
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{1});
}

TEST(NativeSimulation, InitialAndTriggeredTimerStartsUseDispatchWithoutAdvancingLogicDuringSetup) {
    auto data{make_battle()};
    data.runtime.clock_settings.tick_rate = Frequency::make(10);
    auto& definitions{data.runtime.level_events};
    auto const first_timer{data.runtime.timers.add()};
    auto const second_timer{data.runtime.timers.add()};
    auto const first_elapsed{data.runtime.expressions.add_timer_elapsed(first_timer)};
    auto const second_elapsed{data.runtime.expressions.add_timer_elapsed(second_timer)};
    auto const initial_start{definitions.add_start_timer_event({first_timer, SimTick{2}})};
    auto const later_start{definitions.add_start_timer_event({second_timer, SimTick{3}})};
    definitions.initial_events.push_back(initial_start);
    definitions.add_trigger(first_elapsed, std::array{later_start});
    auto const stop{definitions.add_stop_event(telemetry::RunEndReason::DurationReached)};
    definitions.add_trigger(second_elapsed, std::array{stop});

    LevelSim simulation{std::move(data).compile()};
    EXPECT_TRUE(std::ranges::equal(simulation.events().pending_tick_end_events(),
                                   std::array{initial_start}));
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.state(), LevelState::Paused);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{});
    EXPECT_EQ(simulation.expressions().result(first_elapsed), ExprResult::Pending);
    EXPECT_TRUE(simulation.events().pending_tick_end_events().empty());
    simulation.advance(10.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{});

    simulation.start();
    simulation.advance(0.1);
    EXPECT_TRUE(
        std::ranges::equal(simulation.events().tick_end_events(), std::array{initial_start}));
    simulation.advance(0.1);
    EXPECT_TRUE(std::ranges::equal(simulation.events().tick_end_events(),
                                   std::array{initial_start, later_start}));
    EXPECT_EQ(simulation.expressions().result(second_elapsed), ExprResult::Pending);
    simulation.advance(0.2);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    simulation.advance(0.1);
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{5});
    EXPECT_TRUE(std::ranges::equal(simulation.events().tick_end_events(),
                                   std::array{initial_start, later_start, stop}));
}

TEST(NativeSimulation, InitialEventsPublishSpawnsAndRetainStrandedEventsAfterTerminalDispatch) {
    auto data{make_battle().compile()};
    auto& definitions{data.level_events};
    definitions.initial_events.clear();
    definitions.spawn_groups = {};
    definitions.add_spawn_group(EntityType::CapitalShip, 0, 1);
    definitions.add_spawn_group(EntityType::CapitalShip, 1, 1);
    auto const first_spawn{definitions.add_spawn_event({0, 1})};
    auto const second_spawn{definitions.add_spawn_event({1, 1})};
    auto const start{definitions.add_start_timer_event({data.timers.add(), SimTick{1}})};
    auto const stop{definitions.add_stop_event(telemetry::RunEndReason::WorldEnd)};
    auto const stranded{definitions.add_start_timer_event({data.timers.add(), SimTick{1}})};
    definitions.initial_events = {second_spawn, first_spawn, start, stop, stranded};

    LevelSim simulation{std::move(data)};
    EXPECT_EQ(simulation.capital_ships().num_instances(), 0);
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.state(), LevelState::Stopped);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 2);
    expect_health_mappings(simulation);
    EXPECT_TRUE(std::ranges::equal(simulation.entity_ledger().unique_entities().teams(),
                                   std::array{Team::White, Team::Green}));
    EXPECT_TRUE(std::ranges::equal(simulation.events().preparation_events(),
                                   std::array{second_spawn, first_spawn}));
    EXPECT_TRUE(simulation.events().pending_preparation_events().empty());
    EXPECT_TRUE(std::ranges::equal(simulation.events().tick_end_events(),
                                   std::array{start, stop, stranded}));
    EXPECT_TRUE(
        std::ranges::equal(simulation.events().pending_tick_end_events(), std::array{stranded}));
    auto const record{simulation.take_finalized_telemetry_run()};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->completion.reason, telemetry::RunEndReason::WorldEnd);
    EXPECT_EQ(record->completion.completed_ticks, SimTick{});
    simulation.advance(1.0);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{});
    EXPECT_TRUE(
        std::ranges::equal(simulation.events().pending_tick_end_events(), std::array{stranded}));
}

TEST(NativeSimulation, RunsWithoutACompletionOrDurationEvent) {
    auto data{make_battle()};
    data.runtime.clock_settings.tick_rate = Frequency::make(10);
    LevelSim simulation{std::move(data).compile()};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{10});
}

TEST(NativeSimulation, QueuedPausePreservesPendingSpawnsAndTelemetryUntilResume) {
    auto data{make_scheduled_battle().compile()};
    data.telemetry_recording.interval_ticks = 1;
    std::array const pause{EventHandle{0, EventKind::Pause}};
    add_timed_events(data, Duration::make(0.21), pause);
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
    simulation.advance(simulation.clock().tick_period());
    EXPECT_EQ(simulation.state(), LevelState::Running);
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{5});
    EXPECT_EQ(simulation.capital_ships().num_instances(), 2);
    EXPECT_EQ(simulation.events().preparation_events().size(), 2);
    EXPECT_TRUE(simulation.events().pending_preparation_events().empty());
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
    data.runtime.clock_settings.tick_rate = ioj::Frequency::make(10.0);
    LevelSim simulation{std::move(data).compile()};
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
    LevelSim simulation{std::move(data).compile()};
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
    add_turret_spawn(data,
                     {{0.f, -1000.f, 0.f}},
                     {0.f, 90.f, 0.f},
                     Team::Green,
                     ioj::settings::PositiveHealth::make(ioj::sim::Health{20}),
                     ioj::NonNegative<std::int32_t>::make(5));
    add_turret_spawn(data,
                     {{0.f, 1000.f, 0.f}},
                     {},
                     Team::White,
                     ioj::settings::PositiveHealth::make(ioj::sim::Health{30}),
                     ioj::NonNegative<std::int32_t>::make(7));
    auto compiled{std::move(data).compile()};
    auto const initial_events{compiled.level_events.initial_events};
    LevelSim simulation{std::move(compiled)};
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 1)
        << "Only the player is constructed directly";
    EXPECT_TRUE(
        std::ranges::equal(simulation.events().pending_preparation_events(), initial_events));
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.clock().get_completed_ticks(), SimTick{});
    EXPECT_TRUE(simulation.events().pending_preparation_events().empty());
    EXPECT_TRUE(std::ranges::equal(simulation.events().preparation_events(), initial_events));
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
    data.runtime.player->health = {Health{50}, Health{100}};

    LevelSim simulation{std::move(data).compile()};
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
    add_turret_spawn(data,
                     {{0.f, 2000.f, 0.f}},
                     {},
                     Team::Green,
                     ioj::settings::PositiveHealth::make(ioj::sim::Health{100}),
                     ioj::NonNegative<std::int32_t>::make(0));
    add_turret_spawn(data,
                     {{0.f, 3000.f, 0.f}},
                     {},
                     Team::White,
                     ioj::settings::PositiveHealth::make(ioj::sim::Health{100}),
                     ioj::NonNegative<std::int32_t>::make(0));
    add_turret_spawn(data,
                     {{0.f, 4000.f, 0.f}},
                     {},
                     Team::Green,
                     ioj::settings::PositiveHealth::make(ioj::sim::Health{100}),
                     ioj::NonNegative<std::int32_t>::make(0));

    data.runtime.capital_ships.fighter_spawn_slots = 2;
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.resize(2);
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms[0].location.y() = -100.0;
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms[1].location.y() = 100.0;
    data.runtime.fighters.health = ::ioj::settings::PositiveHealth::make(Health{100});
    data.runtime.fighters.speed = 0.f;
    for (auto& entity : data.entities) {
        if (entity.archetype == levels::EntityArchetype::CapitalShip) {
            entity.overrides.initial_spawn_delay = NonNegativeFinite<float>{0.f};
        }
    }
    auto const scheduled{add_turret_spawn(data,
                                          {0.f, 6000.f, 0.f},
                                          {},
                                          Team::White,
                                          settings::PositiveHealth{Health{75}},
                                          NonNegative<std::int32_t>{0})};
    data.entities[scheduled].spawn_time_seconds =
        NonNegativeFinite<double>::make(4.0 / data.runtime.clock_settings.tick_rate.hz());

    LevelSim simulation{std::move(data).compile()};
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

TEST(NativeSimulation, LevelSimPlanarMovementOffsetTest) {
    auto data{make_battle()};
    add_player_spawn(data, {});
    data.runtime.player->flight_models.initial_slot = player::FlightModelSlot::Fourth;
    auto& gunship{player::flight_model_profile(data.runtime.player->flight_models,
                                               player::FlightModelSlot::Fourth)};
    gunship.config.translation.right.manual.response.mode = player::ResponseMode::Direct;
    gunship.config.translation.up.manual.response.mode = player::ResponseMode::Direct;

    LevelSim simulation{std::move(data).compile()};
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
    LevelSim simulation{make_overlap_response_battle().compile()};
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

TEST(NativeSimulation, SimulationsKeepClocksAndEntitiesIndependent) {
    LevelSim first{make_battle().compile()};
    LevelSim second{make_battle().compile()};
    first.finish_initialisation();
    second.finish_initialisation();
    first.start();
    second.start();
    auto const dt{first.clock().tick_period()};
    first.advance(dt);
    EXPECT_EQ(second.clock().get_completed_ticks(), SimTick{0});

    kill_enemy(first);
    first.advance(dt);
    second.advance(dt);
    EXPECT_EQ(first.capital_ships().num_instances(), 1);
    EXPECT_EQ(second.capital_ships().num_instances(), 2);
}

TEST(NativeSimulation, LevelTelemetryRunRecordTest) {

    auto data{make_battle()};
    LevelSim simulation{std::move(data).compile()};
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
