#include <sandbox/simulation/entities/DirectDamageEvents.h>
#include <sandbox/simulation/simulation/LevelSimulation.h>
#include "support/simulation_test_support.h"

namespace {
auto make_cap_battle(std::span<ml::simulation::Team const> const capital_teams,
                     std::span<ml::simulation::Team const> const participating_teams,
                     std::int32_t const max_live_fighters,
                     std::int32_t const spawn_slots,
                     float const spawn_cooldown = 60.f) -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {{1000.f, 1000.f, 1000.f}};
    data.lasers.n_preallocated_instances = 32;
    data.capital_radius = 10.f;
    data.fighter_radius = 1.f;
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
    data.capital_spawns.add_defaulted(capital_count);
    data.capital_target_spawn_indices.resize(static_cast<std::size_t>(capital_count));
    for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        data.capital_spawns.locations.xs[capital_index] = capital_index * 1000.f;
        data.capital_spawns.teams[capital_index] =
            static_cast<ml::simulation::Team>(capital_teams[capital_index]);
        data.capital_spawns.healths[capital_index] = 100;
        data.capital_spawns.initial_spawn_delays[capital_index] = 0.f;
        data.capital_spawns.spawn_cooldowns[capital_index] = spawn_cooldown;
        data.capital_target_spawn_indices[capital_index] = capital_index;
    }

    auto const bounds_count{data.entity_bounds.num()};
    for (std::int32_t index{}; index < bounds_count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }
    return data;
}

void start_and_tick(FLevelSimulation& simulation) {
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
}

auto count_team(FLevelSimulation const& simulation, ml::simulation::Team const team)
    -> std::int32_t {
    std::int32_t count{};
    for (auto const fighter_team : simulation.get_capital_ship_fighters().get_teams()) {
        count += fighter_team == team ? 1 : 0;
    }
    return count;
}
}

TEST(FighterLiveCap, TeamPartitionsAndRemainders) {

    std::vector<ml::simulation::Team> all_teams{ml::simulation::Team::White,
                                                ml::simulation::Team::Red,
                                                ml::simulation::Team::Green,
                                                ml::simulation::Team::Blue,
                                                ml::simulation::Team::Orange,
                                                ml::simulation::Team::Yellow};
    for (auto const team_count : {1, 2, 4, 6}) {
        auto const participants{std::span<ml::simulation::Team const>{all_teams}.first(team_count)};
        auto const remainder{team_count > 1 ? team_count - 1 : 0};
        auto data{make_cap_battle(participants, participants, team_count * 3 + remainder, 4)};
        FLevelSimulation simulation{std::move(data)};
        start_and_tick(simulation);
        ml::simulation_tests::expect_equal(
            simulation.get_capital_ship_fighters().get_num_instances(),
            team_count * 3,
            "Floor partition leaves the global remainder unused");
        for (auto const team : participants) {
            ml::simulation_tests::expect_equal(
                count_team(simulation, team), 3, "Each participant receives the same partition");
        }
    }

    std::vector<ml::simulation::Team> const one_capital{ml::simulation::Team::White};
    std::vector<ml::simulation::Team> const two_participants{ml::simulation::Team::White,
                                                             ml::simulation::Team::Red};
    auto data{make_cap_battle(one_capital, two_participants, 7, 8)};
    FLevelSimulation simulation{std::move(data)};
    start_and_tick(simulation);
    ml::simulation_tests::expect_equal(
        simulation.get_capital_ship_fighters().get_num_instances(),
        3,
        "A team cannot borrow another participant's unused allocation");

    std::vector<ml::simulation::Team> const repeated_capital_teams{
        ml::simulation::Team::White, ml::simulation::Team::White, ml::simulation::Team::Red};
    auto inferred_data{
        make_cap_battle(repeated_capital_teams, std::span<ml::simulation::Team const>{}, 14, 8)};
    FLevelSimulation inferred_simulation{std::move(inferred_data)};
    start_and_tick(inferred_simulation);
    ml::simulation_tests::expect_equal(
        inferred_simulation.get_capital_ship_fighters().get_num_instances(),
        14,
        "Legacy team inference deduplicates team sources");

    FLevelSimulation empty_simulation{make_cap_battle(
        std::span<ml::simulation::Team const>{}, std::span<ml::simulation::Team const>{}, 10, 0)};
    empty_simulation.finish_initialisation();
    empty_simulation.start();
    empty_simulation.advance(empty_simulation.get_clock().get_tick_period());
    ml::simulation_tests::expect_equal(
        empty_simulation.get_capital_ship_fighters().get_num_instances(),
        0,
        "A zero-team simulation remains empty");
}

TEST(FighterLiveCap, PartialWavesPreserveOwnership) {

    std::vector<ml::simulation::Team> const capitals{ml::simulation::Team::White,
                                                     ml::simulation::Team::White};
    std::vector<ml::simulation::Team> const participants{ml::simulation::Team::White};
    auto data{make_cap_battle(capitals, participants, 6, 4)};
    FLevelSimulation simulation{std::move(data)};
    start_and_tick(simulation);
    ml::simulation_tests::expect_equal(simulation.get_capital_ship_fighters().get_num_instances(),
                                       6,
                                       "Full and partial waves stop at the team cap");

    simulation.advance(simulation.get_clock().get_tick_period());
    auto const& capital_simulation{simulation.get_capital_ships()};
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(capital_simulation.get_fighter_handles(0).size()),
        4,
        "First capital owns its full accepted wave");
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(capital_simulation.get_fighter_handles(1).size()),
        2,
        "Second capital owns only its accepted prefix");

    auto parent_death_data{make_cap_battle(capitals, participants, 2, 1)};
    FLevelSimulation parent_death_simulation{std::move(parent_death_data)};
    parent_death_simulation.finish_initialisation();
    parent_death_simulation.start();
    DirectDamageEvents capital_damage;
    capital_damage.add_uninitialised(1);
    capital_damage.damaged_entities[0] = parent_death_simulation.get_capital_ships().get_handle(0);
    capital_damage.instigators[0] = parent_death_simulation.get_capital_ships().get_handle(1);
    capital_damage.damage_amounts[0] = 100;
    parent_death_simulation.get_entity_registry().queue_direct_damage_events(capital_damage);
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(
            parent_death_simulation.get_capital_ships().get_fighter_handles(0).size()),
        2,
        "A surviving same-team capital adopts a new fighter from a dead parent");
}

TEST(FighterLiveCap, DeferredRemovalAndReconstruction) {

    std::vector<ml::simulation::Team> const capitals{ml::simulation::Team::White};
    auto make_data{[&] { return make_cap_battle(capitals, capitals, 1, 1, 0.f); }};
    FLevelSimulation simulation{make_data()};
    start_and_tick(simulation);
    ml::simulation_tests::expect_equal(simulation.get_capital_ship_fighters().get_num_instances(),
                                       1,
                                       "Initial fighter fills the budget");

    DirectDamageEvents damage;
    damage.add_uninitialised(1);
    damage.damaged_entities[0] = simulation.get_capital_ship_fighters().get_handles()[0];
    damage.instigators[0] = simulation.get_capital_ships().get_handle(0);
    damage.damage_amounts[0] = 100000;
    simulation.get_entity_registry().queue_direct_damage_events(damage);
    simulation.advance(simulation.get_clock().get_tick_period());
    ml::simulation_tests::expect_equal(simulation.get_capital_ship_fighters().get_num_instances(),
                                       0,
                                       "Deferred removal finishes before capacity is reusable");
    simulation.advance(simulation.get_clock().get_tick_period());
    ml::simulation_tests::expect_equal(simulation.get_capital_ship_fighters().get_num_instances(),
                                       1,
                                       "Exactly one replacement uses the released slot");

    std::optional<FLevelSimulation> reconstructed;
    reconstructed.emplace(make_data());
    start_and_tick(*reconstructed);
    ml::simulation_tests::expect_equal(
        reconstructed->get_capital_ship_fighters().get_num_instances(),
        1,
        "Reconstruction starts with a fresh budget");
}

TEST(FighterLiveCap, UnknownTeamRejected) {

    std::vector<ml::simulation::Team> const capitals{ml::simulation::Team::Green};
    std::vector<ml::simulation::Team> const participants{ml::simulation::Team::White};
    auto data{make_cap_battle(capitals, participants, 10, 1)};
    FLevelSimulation simulation{std::move(data)};
    start_and_tick(simulation);
    ml::simulation_tests::expect_equal(simulation.get_capital_ship_fighters().get_num_instances(),
                                       0,
                                       "Unknown team creates no fighter");
}
