#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <array>
#include <type_traits>

namespace ioj::sim::tests {

namespace {
auto make_cap_battle(std::span<Team const> const capital_teams,
                     std::int32_t const max_live_auto_spawned_fighters,
                     std::int32_t const spawn_slots,
                     float const spawn_cooldown = 60.f) -> SimulationScenario {
    SimulationScenario data;
    data.runtime.collision.grid_spec = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(32);
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        ioj::Positive<std::int32_t>{1};
    data.runtime.fighters.health = ::ioj::settings::PositiveHealth::make(Health{1000});
    data.runtime.fighters.max_live_auto_spawned_fighters =
        ::ioj::NonNegative<std::int32_t>::make(max_live_auto_spawned_fighters);
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.resize(spawn_slots);

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

TEST(FighterLiveCap, FirstQueuedCarrierGetsSlotFreedDuringResolution) {
    std::array const carriers{Team::Blue, Team::Blue};
    LevelSim simulation{make_cap_battle(carriers, 3, 2, 0.f).compile()};
    start_and_tick(simulation);
    ASSERT_EQ(simulation.fighters().num_instances(), 3u);

    auto const issued{simulation.entity_ledger().issued_counts()[EntityType::Fighter]};
    auto const victim{simulation.fighters().entities().entity_ids().front()};
    DirectDamageEvents damage;
    damage.add(victim, 10000, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());

    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    ASSERT_EQ(simulation.fighters().num_instances(), 2u);
    simulation.advance(dt);
    auto const fighters{simulation.fighters().entities()};
    ASSERT_EQ(fighters.num(), 3u);
    auto const ids{fighters.entity_ids()};
    auto const replacement{
        std::ranges::find_if(ids, [issued](EntityId id) { return id.index() == issued; })};
    ASSERT_NE(replacement, ids.end());
    auto const row{static_cast<std::uint32_t>(replacement - ids.begin())};
    EXPECT_NEAR(vector_at(fighters.view_locations(), row).x(), 0.f, 100.f);
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
    LevelSim zero_wave{make_cap_battle(one_capital, 7, 0).compile()};
    start_and_tick(zero_wave);
    EXPECT_EQ(zero_wave.fighters().num_instances(), 0);

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

    auto mixed{make_cap_battle(one_capital, 2, 2, 0.f)};
    for (int i{}; i < 3; ++i) {
        mixed.entities.push_back({
            .id = levels::EntityId{std::to_string(i)},
            .archetype = levels::EntityArchetype::Fighter,
            .team = Team::White,
            .position = {2000., 1000. * i, 0.},
            .rotation = {0., 90., 0.},
        });
    }
    LevelSim mixed_sim{std::move(mixed).compile()};
    auto const placed{mixed_sim.fighters().entities()};
    ASSERT_EQ(placed.num(), 3);
    EXPECT_EQ(placed.entity_ids()[0], LevelSimTestAccess::level_entity_ids(mixed_sim)[1]);
    EXPECT_EQ(vector_at(placed.view_locations(), 1), (Vector3f{2000.f, 1000.f, 0.f}));
    EXPECT_NEAR(vector_at(placed.view_aim_directions(), 0).y(), 1.f, 1e-5f);
    auto const placed_id{placed.entity_ids()[0]};
    start_and_tick(mixed_sim);
    ASSERT_EQ(mixed_sim.fighters().num_instances(), 5);
    auto const launched_id{mixed_sim.fighters().entities().entity_ids()[3]};
    auto const kill_and_advance{[&](EntityId id) {
        DirectDamageEvents damage;
        damage.add(id, 10000, {});
        LevelSimTestAccess::queue_direct_damage_events(mixed_sim, damage.const_view());
        auto const dt{mixed_sim.clock().tick_period()};
        for (int tick{}; tick < 4; ++tick) {
            mixed_sim.advance(dt);
        }
    }};
    kill_and_advance(placed_id);
    EXPECT_EQ(mixed_sim.fighters().num_instances(), 4);
    EXPECT_EQ(mixed_sim.entity_ledger().issued_counts()[EntityType::Fighter], 5);
    kill_and_advance(launched_id);
    EXPECT_EQ(mixed_sim.fighters().num_instances(), 4);
    EXPECT_EQ(mixed_sim.entity_ledger().issued_counts()[EntityType::Fighter], 6);

    LevelSim empty_simulation{make_cap_battle(std::span<Team const>{}, 10, 0).compile()};

    auto const empty_clk_period{empty_simulation.clock().tick_period()};
    empty_simulation.start();
    empty_simulation.advance(empty_clk_period);
    EXPECT_EQ(empty_simulation.fighters().num_instances(), 0)
        << "A zero-team simulation remains empty";
}

TEST(FighterLiveCap, DerivedParticipantsIncludeFutureSpawns) {
    std::vector<Team> const capitals{Team::Blue};
    auto initial_only{make_cap_battle(capitals, 7, 8)};
    LevelSim initial_simulation{std::move(initial_only).compile()};
    start_and_tick(initial_simulation);
    EXPECT_EQ(count_team(initial_simulation, Team::Blue), 7);

    auto with_reinforcements{make_cap_battle(capitals, 7, 8)};
    auto const turret{add_turret_spawn(with_reinforcements,
                                       {0.f, 6000.f, 0.f},
                                       {},
                                       Team::Red,
                                       settings::PositiveHealth{Health{100}},
                                       NonNegative<std::int32_t>{0})};
    with_reinforcements.entities[turret].spawn_time_seconds = NonNegativeFinite<double>::make(
        100.0 / with_reinforcements.runtime.clock_settings.tick_rate.hz());

    auto const spinner{add_spinner_spawn(with_reinforcements, {2000.f, 0.f, 0.f}, 0.f, 0)};
    with_reinforcements.entities[spinner].spawn_time_seconds =
        with_reinforcements.entities[turret].spawn_time_seconds;

    LevelSim scheduled_simulation{std::move(with_reinforcements).compile()};
    start_and_tick(scheduled_simulation);
    EXPECT_EQ(count_team(scheduled_simulation, Team::Blue), 2)
        << "Blue shares the carrier allowance with the scheduled red turret and white spinner";
    EXPECT_EQ(scheduled_simulation.read_access().turrets().get_entities().num(), 0);
}

} // namespace tests
