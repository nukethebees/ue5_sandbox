#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <type_traits>

namespace ioj::sim::tests {

namespace {
auto make_cap_battle(std::span<Team const> const capital_teams,
                     std::int32_t const max_live_fighters,
                     std::int32_t const spawn_slots,
                     float const spawn_cooldown = 60.f) -> SimulationScenario {
    SimulationScenario data;
    data.runtime.collision.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(32);
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        ioj::Positive<std::int32_t>{1};
    data.runtime.fighters.health = ::ioj::settings::PositiveHealth::make(Health{1000});
    data.runtime.fighters.max_live_fighters =
        ::ioj::NonNegative<std::int32_t>::make(max_live_fighters);
    data.runtime.capital_ships.fighter_spawn_slots = spawn_slots;
    for (std::int32_t slot_index{}; slot_index < spawn_slots; ++slot_index) {
        data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.push_back(
            {.location = {100.0 + slot_index * 10.0, 0.0, 0.0}});
    }

    auto const capital_count{static_cast<std::int32_t>(capital_teams.size())};
    for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        add_capital_spawn(data,
                          {{capital_index * 1000.f, 0.f, 0.f}},
                          capital_teams[capital_index],
                          0.f,
                          spawn_cooldown,
                          ioj::settings::PositiveHealth::make(ioj::sim::Health{100}));
    }

    for (auto const type : EnumTraits<EntityType>::values) {
        data.runtime.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}

void start_and_tick(LevelSim& simulation) {
    simulation.finish_initialisation();
    auto const clk_period{simulation.clock().tick_period()};
    simulation.start();
    simulation.advance(clk_period);
    simulation.advance(clk_period);
}

auto count_team(LevelSim const& simulation, Team const team) -> std::int32_t {
    std::int32_t count{};
    for (auto const fighter_team : simulation.fighters().entities().teams()) {
        count += fighter_team == team ? 1 : 0;
    }
    return count;
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
        LevelSim simulation{std::move(data).compile()};
        start_and_tick(simulation);
        EXPECT_EQ(simulation.fighters().num_instances(), team_count * 3)
            << "Floor partition leaves the global remainder unused";
        for (auto const team : participants) {
            EXPECT_EQ(count_team(simulation, team), 3)
                << "Each participant receives the same partition";
        }
    }

    std::vector<Team> const one_capital{Team::White};
    auto data{make_cap_battle(one_capital, 7, 8)};
    LevelSim simulation{std::move(data).compile()};
    start_and_tick(simulation);
    EXPECT_EQ(simulation.fighters().num_instances(), 7)
        << "An absent team does not reserve fighter capacity";

    std::vector<Team> const repeated_capital_teams{Team::White, Team::White, Team::Red};
    auto inferred_data{make_cap_battle(repeated_capital_teams, 14, 8)};
    LevelSim inferred_simulation{std::move(inferred_data).compile()};
    start_and_tick(inferred_simulation);
    EXPECT_EQ(inferred_simulation.fighters().num_instances(), 14)
        << "Compiled team inference deduplicates team sources";

    LevelSim empty_simulation{make_cap_battle(std::span<Team const>{}, 10, 0).compile()};
    empty_simulation.finish_initialisation();
    auto const empty_clk_period{empty_simulation.clock().tick_period()};
    empty_simulation.start();
    empty_simulation.advance(empty_clk_period);
    EXPECT_EQ(empty_simulation.fighters().num_instances(), 0)
        << "A zero-team simulation remains empty";
}

TEST(FighterLiveCap, DerivedParticipantsIncludeFutureSpawns) {
    std::vector<Team> const capitals{Team::White};
    auto initial_only{make_cap_battle(capitals, 7, 8)};
    LevelSim initial_simulation{std::move(initial_only).compile()};
    start_and_tick(initial_simulation);
    EXPECT_EQ(count_team(initial_simulation, Team::White), 7);

    auto with_reinforcements{make_cap_battle(capitals, 7, 8)};
    auto const turret{add_turret_spawn(with_reinforcements,
                                       {0.f, 6000.f, 0.f},
                                       {},
                                       Team::Red,
                                       settings::PositiveHealth{Health{100}},
                                       NonNegative<std::int32_t>{0})};
    with_reinforcements.entities[turret].spawn_time_seconds = NonNegativeFinite<double>::make(
        100.0 / with_reinforcements.runtime.clock_settings.tick_rate.hz());

    LevelSim scheduled_simulation{std::move(with_reinforcements).compile()};
    start_and_tick(scheduled_simulation);
    EXPECT_EQ(count_team(scheduled_simulation, Team::White), 3)
        << "Reserve the future team's share before its first entity spawns";
    EXPECT_EQ(scheduled_simulation.read_access().turrets().get_entities().num(), 0);
}

TEST(FighterLiveCap, UnknownTeamRejected) {

    std::vector<Team> const capitals{Team::Green};
    auto data{make_cap_battle(capitals, 10, 0)};
    LevelSim simulation{std::move(data).compile()};
    start_and_tick(simulation);
    auto const clk_period{simulation.clock().tick_period()};

    FighterSpawnQueue spawns;
    spawns.add_defaulted(1);
    spawns.view().teams()[0] = Team::White;
    LevelSimTestAccess::queue_fighter_spawns(simulation, spawns.const_view());
    simulation.advance(clk_period);

    EXPECT_EQ(simulation.fighters().num_instances(), 0) << "Unknown team creates no fighter";
}

} // namespace tests
