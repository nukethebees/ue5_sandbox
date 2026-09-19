#include <algorithm>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/level_sim.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

namespace {
auto make_cap_battle(std::span<Team const> const capital_teams,
                     std::span<Team const> const participating_teams,
                     std::int32_t const max_live_fighters,
                     std::int32_t const spawn_slots,
                     float const spawn_cooldown = 60.f) -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.lasers.n_preallocated_instances = 32;
    data.overlap_response.damage_per_overlap_detection = 1;
    data.fighters.health = 1000;
    data.fighters.max_live_fighters = max_live_fighters;
    for (auto const team : participating_teams) {
        data.participating_teams.add(team);
    }
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
                                                  -1,
                                                  0.f,
                                                  spawn_cooldown,
                                                  100)};
        data.level_events.initial_spawns.capital_spawns.get_view()
            .target_entity_indices()[capital_index] = entity_index;
    }

    auto const bounds_count{data.entity_bounds.num()};
    for (std::int32_t index{}; index < bounds_count; ++index) {
        data.entity_bounds.set_half_extents(index, {{10.f, 10.f, 10.f}});
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

void queue_damage(LevelSim& simulation, EntityUniqueId const target, std::int32_t const damage) {
    DirectDamageEvents events;
    events.add_uninitialised(1);
    events.damaged_entities[0] = target;
    events.damage_amounts[0] = damage;
    LevelSimTestAccess::queue_direct_damage_events(simulation, events.get_const_view());
}

auto location_changed(Vector3f const before, Vector3f const after) -> bool {
    return before.X != after.X || before.Y != after.Y || before.Z != after.Z;
}
}

TEST(FighterLiveCap, TeamPartitionsAndRemainders) {

    std::vector<Team> all_teams{
        Team::White, Team::Red, Team::Green, Team::Blue, Team::Orange, Team::Yellow};
    for (auto const team_count : {1, 2, 4, 6}) {
        auto const participants{std::span<Team const>{all_teams}.first(team_count)};
        auto const remainder{team_count > 1 ? team_count - 1 : 0};
        auto data{make_cap_battle(participants, participants, team_count * 3 + remainder, 4)};
        LevelSim simulation{std::move(data)};
        start_and_tick(simulation);
        tests::expect_equal(simulation.get_fighters().get_num_instances(),
                            team_count * 3,
                            "Floor partition leaves the global remainder unused");
        for (auto const team : participants) {
            tests::expect_equal(
                count_team(simulation, team), 3, "Each participant receives the same partition");
        }
    }

    std::vector<Team> const one_capital{Team::White};
    std::vector<Team> const two_participants{Team::White, Team::Red};
    auto data{make_cap_battle(one_capital, two_participants, 7, 8)};
    LevelSim simulation{std::move(data)};
    start_and_tick(simulation);
    tests::expect_equal(simulation.get_fighters().get_num_instances(),
                        3,
                        "A team cannot borrow another participant's unused allocation");

    std::vector<Team> const repeated_capital_teams{Team::White, Team::White, Team::Red};
    auto inferred_data{make_cap_battle(repeated_capital_teams, std::span<Team const>{}, 14, 8)};
    LevelSim inferred_simulation{std::move(inferred_data)};
    start_and_tick(inferred_simulation);
    tests::expect_equal(inferred_simulation.get_fighters().get_num_instances(),
                        14,
                        "Compiled team inference deduplicates team sources");

    LevelSim empty_simulation{
        make_cap_battle(std::span<Team const>{}, std::span<Team const>{}, 10, 0)};
    empty_simulation.finish_initialisation();
    empty_simulation.start();
    empty_simulation.advance(empty_simulation.get_clock().get_tick_period());
    tests::expect_equal(empty_simulation.get_fighters().get_num_instances(),
                        0,
                        "A zero-team simulation remains empty");
}

TEST(FighterLiveCap, PartialWavesPreserveOwnership) {

    std::vector<Team> const capitals{Team::White, Team::White};
    std::vector<Team> const participants{Team::White};
    auto data{make_cap_battle(capitals, participants, 6, 4)};
    LevelSim simulation{std::move(data)};
    start_and_tick(simulation);
    tests::expect_equal(simulation.get_fighters().get_num_instances(),
                        6,
                        "Full and partial waves stop at the team cap");

    simulation.advance(simulation.get_clock().get_tick_period());
    auto const& capital_simulation{simulation.get_capital_ships()};
    tests::expect_equal(static_cast<std::int32_t>(capital_simulation.get_fighter_ids(0).size()),
                        4,
                        "First capital owns its full accepted wave");
    tests::expect_equal(static_cast<std::int32_t>(capital_simulation.get_fighter_ids(1).size()),
                        2,
                        "Second capital owns only its accepted prefix");

    auto parent_death_data{make_cap_battle(capitals, participants, 2, 1)};
    LevelSim parent_death_simulation{std::move(parent_death_data)};
    parent_death_simulation.finish_initialisation();
    parent_death_simulation.start();
    DirectDamageEvents capital_damage;
    capital_damage.add_uninitialised(1);
    capital_damage.damaged_entities[0] =
        parent_death_simulation.get_read_view().capitals.entities.entity_ids[0];
    capital_damage.instigators[0] =
        parent_death_simulation.get_read_view().capitals.entities.entity_ids[1];
    capital_damage.damage_amounts[0] = 100;
    LevelSimTestAccess::queue_direct_damage_events(parent_death_simulation,
                                                   capital_damage.get_const_view());
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    tests::expect_equal(static_cast<std::int32_t>(
                            parent_death_simulation.get_capital_ships().get_fighter_ids(0).size()),
                        2,
                        "A pending launch from a dead parent is reassigned to its surviving team");
    auto const survivor{parent_death_simulation.get_capital_ships().get_id(0)};
    for (auto const parent : parent_death_simulation.get_fighters().get_parent_ids()) {
        tests::expect_equal(parent, survivor, "Pending fighter uses the surviving capital parent");
    }
}

TEST(FighterLiveCap, FinalCapitalDeathOrphansFighters) {

    std::vector<Team> const capitals{Team::White, Team::Red};
    auto data{make_cap_battle(capitals, capitals, 4, 2, 6000.f)};
    auto targets{
        data.level_events.initial_spawns.capital_spawns.get_view().target_entity_indices()};
    targets[0] = 1;
    targets[1] = 0;

    LevelSim simulation{std::move(data)};
    start_and_tick(simulation);

    auto const& capital_simulation{simulation.get_capital_ships()};
    auto const& fighter_simulation{simulation.get_fighters()};
    auto const white_capital{capital_simulation.get_id(0)};
    auto const red_capital{capital_simulation.get_id(1)};
    std::vector<EntityUniqueId> white_fighters;
    for (auto const fighter : fighter_simulation.get_entity_ids()) {
        auto const index{simulation.get_agent_indexes().find(fighter)};
        if (fighter_simulation.get_teams()[index] == Team::White) {
            white_fighters.push_back(fighter);
        }
    }
    tests::expect_equal(static_cast<std::int32_t>(white_fighters.size()),
                        2,
                        "Final capital begins with its fighter wave");

    queue_damage(simulation, white_capital, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());

    tests::expect_true(!capital_simulation.find_first_index_on_team(Team::White).has_value(),
                       "Final capital is removed");
    for (auto const fighter : white_fighters) {
        auto const index{simulation.get_agent_indexes().find(fighter)};
        tests::expect_true(index >= 0, "Orphaned fighter remains registered");
        if (index < 0) {
            continue;
        }
        tests::expect_equal(fighter_simulation.get_teams()[index],
                            Team::White,
                            "Orphaned fighter retains its team");
        tests::expect_true(!fighter_simulation.get_parent_ids()[index].is_valid(),
                           "Orphaned fighter has no capital parent");
        tests::expect_equal(fighter_simulation.get_target_ids()[index],
                            red_capital,
                            "Orphaned fighter retains its target");
        tests::expect_equal(fighter_simulation.get_tasks()[index],
                            FighterTask::Attack,
                            "Orphaned fighter continues its attack task");
        tests::expect_true(!std::ranges::contains(capital_simulation.get_fighter_ids(), fighter),
                           "Orphaned fighter is absent from capital rosters");
    }

    auto const tracked_fighter{white_fighters.front()};
    auto const tracked_index{simulation.get_agent_indexes().find(tracked_fighter)};
    auto const before_location{fighter_simulation.get_locations()[tracked_index]};
    for (std::int32_t tick{}; tick < 10; ++tick) {
        simulation.advance(simulation.get_clock().get_tick_period());
    }
    auto const after_index{simulation.get_agent_indexes().find(tracked_fighter)};
    tests::expect_true(after_index >= 0, "Orphaned fighter remains live while simulating");
    if (after_index >= 0) {
        tests::expect_true(
            location_changed(before_location, fighter_simulation.get_locations()[after_index]),
            "Orphaned fighter continues moving");
    }

    queue_damage(simulation, tracked_fighter, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());
    tests::expect_equal(simulation.get_agent_indexes().find(tracked_fighter),
                        -1,
                        "Orphaned fighter dies through normal damage resolution");
    tests::expect_equal(count_team(simulation, Team::White),
                        1,
                        "Other orphaned fighters remain live after one is destroyed");
}

TEST(FighterLiveCap, PendingLaunchSurvivesFinalCapitalDeathUnowned) {

    std::vector<Team> const capitals{Team::White, Team::Red};
    auto data{make_cap_battle(capitals, capitals, 2, 1, 6000.f)};
    auto targets{
        data.level_events.initial_spawns.capital_spawns.get_view().target_entity_indices()};
    targets[0] = 1;
    targets[1] = 0;

    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto const white_capital{simulation.get_capital_ships().get_id(0)};
    queue_damage(simulation, white_capital, 100000);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.advance(simulation.get_clock().get_tick_period());

    auto const& capital_simulation{simulation.get_capital_ships()};
    auto const& fighter_simulation{simulation.get_fighters()};
    tests::expect_true(!capital_simulation.find_first_index_on_team(Team::White).has_value(),
                       "Spawn parent capital is removed");
    tests::expect_equal(count_team(simulation, Team::White),
                        1,
                        "Pending fighter is admitted after its final parent dies");
    for (auto const fighter : fighter_simulation.get_entity_ids()) {
        auto const index{simulation.get_agent_indexes().find(fighter)};
        if (fighter_simulation.get_teams()[index] != Team::White) {
            continue;
        }
        tests::expect_true(!fighter_simulation.get_parent_ids()[index].is_valid(),
                           "Pending fighter is admitted unowned");
        tests::expect_true(!std::ranges::contains(capital_simulation.get_fighter_ids(), fighter),
                           "Pending fighter is absent from capital rosters");
    }
}

TEST(FighterLiveCap, SameTickRemovalAndReconstruction) {

    std::vector<Team> const capitals{Team::White};
    auto make_data{[&] { return make_cap_battle(capitals, capitals, 1, 1, 0.f); }};
    LevelSim simulation{make_data()};
    start_and_tick(simulation);
    tests::expect_equal(
        simulation.get_fighters().get_num_instances(), 1, "Initial fighter fills the budget");

    DirectDamageEvents damage;
    damage.add_uninitialised(1);
    damage.damaged_entities[0] = simulation.get_fighters().get_entity_ids()[0];
    auto const original_id{simulation.get_read_view().fighters.entities.entity_ids[0]};
    damage.instigators[0] = simulation.get_read_view().capitals.entities.entity_ids[0];
    damage.damage_amounts[0] = 100000;
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.get_const_view());
    simulation.advance(simulation.get_clock().get_tick_period());
    tests::expect_equal(simulation.get_fighters().get_num_instances(),
                        0,
                        "Dead fighter is removed at the end of Resolution");
    EXPECT_EQ(simulation.get_agent_indexes().find(original_id), -1);
    EXPECT_FALSE(simulation.get_agent_accessor().read(original_id));
    EXPECT_TRUE(simulation.get_capital_ships().get_fighter_ids(0).empty());
    FighterOrderQueue stale_orders;
    stale_orders.add(original_id, FighterOrder{.task = 1}, FighterTask::Standby, {});
    LevelSimTestAccess::queue_fighter_orders(simulation, stale_orders);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 0);
    simulation.advance(simulation.get_clock().get_tick_period());
    tests::expect_equal(simulation.get_fighters().get_num_instances(),
                        1,
                        "Exactly one replacement uses the released slot");

    auto const replacement_id{simulation.get_read_view().fighters.entities.entity_ids[0]};
    EXPECT_NE(replacement_id, original_id);
    stale_orders.add(simulation.get_read_view().capitals.entities.entity_ids[0],
                     FighterOrder{.task = 1},
                     FighterTask::Standby,
                     {});
    stale_orders.add(EntityUniqueId::make(100000, EntityType::Fighter),
                     FighterOrder{.task = 1},
                     FighterTask::Standby,
                     {});
    LevelSimTestAccess::queue_fighter_orders(simulation, stale_orders);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Attack);

    FighterOrderQueue valid_orders;
    valid_orders.add(replacement_id, FighterOrder{.task = 1}, FighterTask::Standby, {});
    LevelSimTestAccess::queue_fighter_orders(simulation, valid_orders);
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Attack);
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_tasks()[0], FighterTask::Standby);

    std::optional<LevelSim> reconstructed;
    reconstructed.emplace(make_data());
    start_and_tick(*reconstructed);
    tests::expect_equal(reconstructed->get_fighters().get_num_instances(),
                        1,
                        "Reconstruction starts with a fresh budget");
}

TEST(FighterLiveCap, UnknownTeamRejected) {

    std::vector<Team> const capitals{Team::Green};
    std::vector<Team> const participants{Team::White};
    auto data{make_cap_battle(capitals, participants, 10, 1)};
    LevelSim simulation{std::move(data)};
    start_and_tick(simulation);
    tests::expect_equal(
        simulation.get_fighters().get_num_instances(), 0, "Unknown team creates no fighter");
}

} // namespace tests
