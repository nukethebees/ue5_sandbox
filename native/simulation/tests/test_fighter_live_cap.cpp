#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/fighters/fighter_task_format.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <type_traits>

namespace ioj::sim::tests {

TEST(SimulationSchema, PreservesHealthAndFighterOrderRepresentations) {
    static_assert(std::is_same_v<Health::value_type, std::int32_t>);
    static_assert(sizeof(FighterOrder) == 1 && alignof(FighterOrder) == 1);
    static_assert(std::is_trivially_copyable_v<FighterOrder>);
    static_assert(FighterOrder{}.raw_value() == 0);
    static_assert(static_cast<std::uint8_t>(FighterTask::Standby) == 0);
    static_assert(static_cast<std::uint8_t>(FighterTask::MoveToDestination) == 1);
    static_assert(static_cast<std::uint8_t>(FighterTask::Attack) == 2);
    static_assert(enum_count<FighterTask>() == 3);
    for (std::uint8_t raw{}; raw < 4; ++raw) {
        auto const order{FighterOrder::from_raw(raw)};
        EXPECT_EQ(order.task(), raw & 1);
        EXPECT_EQ(order.target(), raw >> 1);
        EXPECT_EQ((FighterOrder{order.task(), order.target()}).raw_value(), raw);
    }
    EXPECT_EQ(FighterOrder::from_raw(0xfc).raw_value(), 0xfc);
    EXPECT_TRUE(is_dead(Health{-1}));
    EXPECT_TRUE(is_alive(Health{1}));
}

namespace {
auto make_cap_battle(std::span<Team const> const capital_teams,
                     std::int32_t const max_live_fighters,
                     std::int32_t const spawn_slots,
                     float const spawn_cooldown = 60.f) -> LevelSimInitData {
    LevelSimInitData data;
    data.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(32);
    data.collision.overlap_response.damage_per_overlap_detection = 1;
    data.fighters.health = ::ioj::settings::PositiveHealth::make(Health{1000});
    data.fighters.max_live_fighters = ::ioj::NonNegative<std::int32_t>::make(max_live_fighters);
    data.capital_ships.fighter_spawn_slots = spawn_slots;
    for (std::int32_t slot_index{}; slot_index < spawn_slots; ++slot_index) {
        data.capital_ships.fighter_spawn_slots_relative_transforms.push_back(
            {.location = {100.0 + slot_index * 10.0, 0.0, 0.0}});
    }

    auto const capital_count{static_cast<std::int32_t>(capital_teams.size())};
    for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        auto const entity_index{add_capital_spawn(data,
                                                  {{capital_index * 1000.f, 0.f, 0.f}},
                                                  capital_teams[capital_index],
                                                  invalid_level_entity_index,
                                                  0.f,
                                                  spawn_cooldown,
                                                  100)};
        // add_capital_spawn may grow and relocate the storage.
        // NOLINTBEGIN(ioj-loop-view-accessor-call)
        data.level_events.schedule.capital_spawns.get_view()
            .target_entity_indices()[capital_index] = entity_index;
        // NOLINTEND(ioj-loop-view-accessor-call)
    }

    for (auto const type : EnumTraits<EntityType>::values) {
        data.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}

void start_and_tick(LevelSim& simulation) {
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.advance(simulation.get_clock().get_tick_period());
}

auto count_team(LevelSim const& simulation, Team const team) -> std::int32_t {
    std::int32_t count{};
    for (auto const fighter_team : simulation.get_fighters().get_teams()) {
        count += fighter_team == team ? 1 : 0;
    }
    return count;
}

void queue_damage(LevelSim& simulation,
                  EntityId const target,
                  std::int32_t const damage,
                  EntityId const instigator = {}) {
    DirectDamageEvents events;
    events.add(target, damage, instigator);
    LevelSimTestAccess::queue_direct_damage_events(simulation, events.get_const_view());
}
}

TEST(FighterLiveCap, TeamPartitionsAndRemainders) {

    std::vector<Team> all_teams{
        Team::White, Team::Red, Team::Green, Team::Blue, Team::Orange, Team::Yellow};
    auto const team_view{std::span<Team const>{all_teams}};
    for (auto const team_count : {1, 2, 4, 6}) {
        // Each test case selects a different participant count.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const participants{team_view.first(team_count)};
        auto const remainder{team_count > 1 ? team_count - 1 : 0};
        auto data{make_cap_battle(participants, team_count * 3 + remainder, 4)};
        GameMemory simulation_memory;
        LevelSim simulation{simulation_memory, std::move(data)};
        start_and_tick(simulation);
        EXPECT_EQ(simulation.get_fighters().get_num_instances(), team_count * 3)
            << "Floor partition leaves the global remainder unused";
        for (auto const team : participants) {
            EXPECT_EQ(count_team(simulation, team), 3)
                << "Each participant receives the same partition";
        }
    }

    std::vector<Team> const one_capital{Team::White};
    auto data{make_cap_battle(one_capital, 7, 8)};
    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, std::move(data)};
    start_and_tick(simulation);
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 7)
        << "An absent team does not reserve fighter capacity";

    std::vector<Team> const repeated_capital_teams{Team::White, Team::White, Team::Red};
    auto inferred_data{make_cap_battle(repeated_capital_teams, 14, 8)};
    GameMemory inferred_simulation_memory;
    LevelSim inferred_simulation{inferred_simulation_memory, std::move(inferred_data)};
    start_and_tick(inferred_simulation);
    EXPECT_EQ(inferred_simulation.get_fighters().get_num_instances(), 14)
        << "Compiled team inference deduplicates team sources";

    GameMemory empty_simulation_memory;
    LevelSim empty_simulation{empty_simulation_memory,
                              make_cap_battle(std::span<Team const>{}, 10, 0)};
    empty_simulation.finish_initialisation();
    empty_simulation.start();
    empty_simulation.advance(empty_simulation.get_clock().get_tick_period());
    EXPECT_EQ(empty_simulation.get_fighters().get_num_instances(), 0)
        << "A zero-team simulation remains empty";
}

TEST(FighterLiveCap, DerivedParticipantsIncludeFutureSpawns) {
    std::vector<Team> const capitals{Team::White};
    auto initial_only{make_cap_battle(capitals, 7, 8)};
    GameMemory initial_simulation_memory;
    LevelSim initial_simulation{initial_simulation_memory, std::move(initial_only)};
    start_and_tick(initial_simulation);
    EXPECT_EQ(count_team(initial_simulation, Team::White), 7);

    auto with_reinforcements{make_cap_battle(capitals, 7, 8)};
    auto& schedule{with_reinforcements.level_events.schedule};
    schedule.execution_ticks = {SimTick{100}};
    schedule.event_group_counts = {{}};
    auto const payload_offset{schedule.turret_spawns.num()};
    schedule.turret_spawns.add_defaulted(1);
    auto const turret{schedule.turret_spawns.get_view(payload_offset, 1)};
    turret.entity_indices()[0] = with_reinforcements.level_events.initialisation.entity_count++;
    turret.teams()[0] = Team::Red;
    turret.healths()[0] = Health{100};
    set_vector(turret.view_locations(), 0, {{0.f, 6000.f, 0.f}});
    ASSERT_TRUE(schedule.add_spawn_group(EntityType::Turret, payload_offset, 1));

    GameMemory scheduled_simulation_memory;
    LevelSim scheduled_simulation{scheduled_simulation_memory, std::move(with_reinforcements)};
    start_and_tick(scheduled_simulation);
    EXPECT_EQ(count_team(scheduled_simulation, Team::White), 3)
        << "Reserve the future team's share before its first entity spawns";
    EXPECT_EQ(scheduled_simulation.get_read_access().get_turrets().get_entities().num(), 0);
}

TEST(FighterLiveCap, PartialWavesPreserveOwnership) {

    std::vector<Team> const capitals{Team::White, Team::White};
    auto data{make_cap_battle(capitals, 6, 4)};
    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, std::move(data)};
    start_and_tick(simulation);
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 6)
        << "Full and partial waves stop at the team cap";

    simulation.advance(simulation.get_clock().get_tick_period());
    auto const& capital_simulation{simulation.get_capital_ships()};
    EXPECT_EQ(static_cast<std::int32_t>(capital_simulation.get_fighter_ids(0).size()), 4)
        << "First capital owns its full accepted wave";
    EXPECT_EQ(static_cast<std::int32_t>(capital_simulation.get_fighter_ids(1).size()), 2)
        << "Second capital owns only its accepted prefix";

    auto parent_death_data{make_cap_battle(capitals, 2, 1)};
    GameMemory parent_death_simulation_memory;
    LevelSim parent_death_simulation{parent_death_simulation_memory, std::move(parent_death_data)};
    parent_death_simulation.finish_initialisation();
    parent_death_simulation.start();
    auto const& surviving_capitals{parent_death_simulation.get_capital_ships()};
    queue_damage(
        parent_death_simulation, surviving_capitals.get_id(0), 100, surviving_capitals.get_id(1));
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    EXPECT_EQ(static_cast<std::int32_t>(
                  parent_death_simulation.get_capital_ships().get_fighter_ids(0).size()),
              2)
        << "A pending launch from a dead parent is reassigned to its surviving team";
    auto const survivor{parent_death_simulation.get_capital_ships().get_id(0)};
    for (auto const parent : parent_death_simulation.get_fighters().get_parent_ids()) {
        EXPECT_EQ(parent, survivor) << "Pending fighter uses the surviving capital parent";
    }
}

TEST(FighterLiveCap, FinalCapitalDeathOrphansFighters) {

    std::vector<Team> const capitals{Team::White, Team::Red};
    auto data{make_cap_battle(capitals, 4, 2, 6000.f)};
    auto targets{data.level_events.schedule.capital_spawns.get_view().target_entity_indices()};
    targets[0] = 1;
    targets[1] = 0;

    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, std::move(data)};
    start_and_tick(simulation);

    auto const& capital_simulation{simulation.get_capital_ships()};
    auto const& fighter_simulation{simulation.get_fighters()};
    auto const white_capital{capital_simulation.get_id(0)};
    auto const red_capital{capital_simulation.get_id(1)};
    std::vector<EntityId> white_fighters;
    auto const fighter_teams{fighter_simulation.get_teams()};
    for (auto const fighter : fighter_simulation.get_entity_ids()) {
        auto const index{observe_entity_row(simulation, fighter)};
        if (fighter_teams[index] == Team::White) {
            white_fighters.push_back(fighter);
        }
    }
    EXPECT_EQ(static_cast<std::int32_t>(white_fighters.size()), 2)
        << "Final capital begins with its fighter wave";

    queue_damage(simulation, white_capital, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());

    EXPECT_TRUE(!capital_simulation.find_first_index_on_team(Team::White).has_value())
        << "Final capital is removed";
    for (auto const fighter : white_fighters) {
        auto const index{observe_entity_row(simulation, fighter)};
        EXPECT_NE(index, EntityInstanceHandle::invalid_value)
            << "Orphaned fighter remains registered";
        if (index == EntityInstanceHandle::invalid_value) {
            continue;
        }
        EXPECT_EQ(fighter_simulation.get_teams()[index], Team::White)
            << "Orphaned fighter retains its team";
        EXPECT_TRUE(!fighter_simulation.get_parent_ids()[index].is_valid())
            << "Orphaned fighter has no capital parent";
        EXPECT_EQ(fighter_simulation.get_target_ids()[index], red_capital)
            << "Orphaned fighter retains its target";
        EXPECT_EQ(fighter_simulation.get_tasks()[index], FighterTask::Attack)
            << "Orphaned fighter continues its attack task";
        EXPECT_TRUE(!std::ranges::contains(capital_simulation.get_fighter_ids(), fighter))
            << "Orphaned fighter is absent from capital rosters";
    }

    auto const tracked_fighter{white_fighters.front()};
    auto const tracked_index{observe_entity_row(simulation, tracked_fighter)};
    auto const before_location{vector_at(fighter_simulation.get_locations(), tracked_index)};
    for (std::int32_t tick{}; tick < 10; ++tick) {
        simulation.advance(simulation.get_clock().get_tick_period());
    }
    auto const after_index{observe_entity_row(simulation, tracked_fighter)};
    EXPECT_NE(after_index, EntityInstanceHandle::invalid_value)
        << "Orphaned fighter remains live while simulating";
    if (after_index != EntityInstanceHandle::invalid_value) {
        EXPECT_NE(before_location, vector_at(fighter_simulation.get_locations(), after_index))
            << "Orphaned fighter continues moving";
    }

    queue_damage(simulation, tracked_fighter, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(observe_entity_row(simulation, tracked_fighter), EntityInstanceHandle::invalid_value)
        << "Orphaned fighter dies through normal damage resolution";
    EXPECT_EQ(count_team(simulation, Team::White), 1)
        << "Other orphaned fighters remain live after one is destroyed";
}

TEST(FighterLiveCap, PendingLaunchSurvivesFinalCapitalDeathUnowned) {

    std::vector<Team> const capitals{Team::White, Team::Red};
    auto data{make_cap_battle(capitals, 2, 1, 6000.f)};
    auto targets{data.level_events.schedule.capital_spawns.get_view().target_entity_indices()};
    targets[0] = 1;
    targets[1] = 0;

    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto const white_capital{simulation.get_capital_ships().get_id(0)};
    queue_damage(simulation, white_capital, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.advance(simulation.get_clock().get_tick_period());

    auto const& capital_simulation{simulation.get_capital_ships()};
    auto const& fighter_simulation{simulation.get_fighters()};
    EXPECT_TRUE(!capital_simulation.find_first_index_on_team(Team::White).has_value())
        << "Spawn parent capital is removed";
    EXPECT_EQ(count_team(simulation, Team::White), 1)
        << "Pending fighter is admitted after its final parent dies";
    auto const fighter_teams{fighter_simulation.get_teams()};
    for (auto const fighter : fighter_simulation.get_entity_ids()) {
        auto const index{observe_entity_row(simulation, fighter)};
        if (fighter_teams[index] != Team::White) {
            continue;
        }
        EXPECT_TRUE(!fighter_simulation.get_parent_ids()[index].is_valid())
            << "Pending fighter is admitted unowned";
        EXPECT_TRUE(!std::ranges::contains(capital_simulation.get_fighter_ids(), fighter))
            << "Pending fighter is absent from capital rosters";
    }
}

TEST(FighterLiveCap, SameTickRemovalAndReconstruction) {

    std::vector<Team> const capitals{Team::White};
    auto make_data{[&] { return make_cap_battle(capitals, 1, 1, 0.f); }};
    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, make_data()};
    start_and_tick(simulation);
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 1)
        << "Initial fighter fills the budget";

    auto const original_id{simulation.get_fighters().get_entity_ids()[0]};
    queue_damage(simulation, original_id, 100000, simulation.get_capital_ships().get_id(0));
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 0)
        << "Dead fighter is removed at the end of Resolution";
    expect_entity_unregistered(simulation, original_id);
    EXPECT_TRUE(simulation.get_capital_ships().get_fighter_ids(0).empty());
    FighterOrderQueue stale_orders;
    stale_orders.add(original_id, FighterOrder{1, 0}, FighterTask::Standby, {});
    LevelSimTestAccess::queue_fighter_orders(simulation, stale_orders);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 0);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 1)
        << "Exactly one replacement uses the released slot";

    auto const replacement_id{
        simulation.get_read_access().get_fighters().get_entities().entity_ids()[0]};
    EXPECT_NE(replacement_id, original_id);
    stale_orders.add(
        EntityId(100000, EntityType::Fighter), FighterOrder{1, 0}, FighterTask::Standby, {});
    LevelSimTestAccess::queue_fighter_orders(simulation, stale_orders);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Attack);

    FighterOrderQueue valid_orders;
    valid_orders.add(replacement_id, FighterOrder{1, 0}, FighterTask::Standby, {});
    LevelSimTestAccess::queue_fighter_orders(simulation, valid_orders);
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Attack);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Standby);

    GameMemory reconstructed_memory;
    std::optional<LevelSim> reconstructed;
    reconstructed.emplace(reconstructed_memory, make_data());
    start_and_tick(*reconstructed);
    EXPECT_EQ(reconstructed->get_fighters().get_num_instances(), 1)
        << "Reconstruction starts with a fresh budget";
}

TEST(FighterLiveCap, UnknownTeamRejected) {

    std::vector<Team> const capitals{Team::Green};
    auto data{make_cap_battle(capitals, 10, 0)};
    GameMemory simulation_memory;
    LevelSim simulation{simulation_memory, std::move(data)};
    start_and_tick(simulation);

    FighterSpawnQueue spawns;
    spawns.add_defaulted(1);
    spawns.get_view().teams()[0] = Team::White;
    LevelSimTestAccess::queue_fighter_spawns(simulation, spawns.get_const_view());
    simulation.advance(simulation.get_clock().get_tick_period());

    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 0)
        << "Unknown team creates no fighter";
}

} // namespace tests
