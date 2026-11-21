#include "../support/simulation_test_support.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/testing/entity_observations.h>

namespace ioj::sim {
namespace collision_damage_test {
inline constexpr std::int32_t player_health{1000};
inline constexpr std::int32_t capital_health{150};
inline constexpr std::int32_t overlap_damage{50};
}

TEST(WorldlessSpaceGameSimulation, OrchestratorFixedStepPauseResumeAndCatchUp) {
    tests::WorldlessSimulationTest harness{tests::make_simulation_data(tests::make_fixture())};

    auto& simulation{harness.simulation()};
    auto const& clk{simulation.clock()};
    std::int32_t end_tick_calls{};
    harness.on_end_tick = [&](LevelSim&) { ++end_tick_calls; };
    simulation.start();
    auto const period{clk.tick_period()};
    harness.advance(period * 0.5);
    EXPECT_EQ(std::uint64_t{0}, clk.get_completed_ticks().value()) << "Half tick is accumulated";
    harness.advance(period * 0.5);
    EXPECT_EQ(std::uint64_t{1}, clk.get_completed_ticks().value()) << "Two half ticks advance once";
    EXPECT_EQ(1, end_tick_calls) << "End-tick hook executes once per completed tick";
    harness.advance(period * 3.25);
    EXPECT_EQ(std::uint64_t{4}, clk.get_completed_ticks().value())
        << "Large delta catches up deterministically";
    EXPECT_EQ(2, end_tick_calls) << "Catch-up is observed once after the completed advance";
    EXPECT_NEAR(period * 4.0, clk.simulation_time(), 1.e-9)
        << "Simulation time derives from completed ticks";
    simulation.pause();
    harness.advance(period * 10.0);
    EXPECT_EQ(std::uint64_t{4}, clk.get_completed_ticks().value())
        << "Paused simulation ignores time";
    simulation.start();
    harness.advance(period * 0.75);
    EXPECT_EQ(std::uint64_t{5}, clk.get_completed_ticks().value())
        << "Resume preserves accumulated fraction";
    EXPECT_EQ(3, end_tick_calls) << "Resumed tick executes one hook";
}

TEST(WorldlessSpaceGameSimulation, EntitiesNonLethalThenLethalDamageCleansUpAtomically) {
    tests::SimulationScenario data{tests::make_simulation_data(tests::make_fixture())};
    tests::add_capital_spawn(
        data, {}, Team::White, 60.f, 60.f, settings::PositiveHealth{Health{100}});
    tests::WorldlessSimulationTest harness{std::move(data).compile()};
    auto& simulation{harness.simulation()};
    auto const& capitals{simulation.capital_ships()};
    ASSERT_EQ(capitals.num_instances(), 1);
    auto const victim{capitals.get_entities().entity_ids()[0]};
    EXPECT_EQ(capitals.healths().health(victim), Health{100});

    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    harness.queue_damage(std::array{victim}, 25);
    simulation.advance(dt);
    ASSERT_EQ(capitals.num_instances(), 1);
    EXPECT_EQ(capitals.healths().health(victim), Health{75});
    simulation.advance(dt);
    EXPECT_EQ(capitals.healths().health(victim), Health{75}) << "Damage is applied once";

    harness.queue_damage(std::array{victim}, 75);
    simulation.advance(dt);
    EXPECT_EQ(capitals.num_instances(), 0);
    EXPECT_EQ(harness.ledger().count_alive(), 0);
    EXPECT_FALSE(entity_is_alive(simulation, victim));
    EXPECT_EQ(capitals.healths().health(victim), Health{0});
    EXPECT_EQ(harness.ledger().kills(victim), 0u) << "Unattributed death does not create a kill";
}

TEST(WorldlessSpaceGameSimulation, EntitiesCollisionOverlapDamagesAndKills) {
    auto const config{tests::make_fixture()};
    tests::SimulationScenario data{tests::make_simulation_data(config)};
    data.runtime.collision.overlap_response.damage_per_overlap_detection =
        Positive<std::int32_t>{collision_damage_test::overlap_damage};
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_player_spawn(data, tests::make_player_spawn(config));
    data.runtime.player->health = {Health{collision_damage_test::player_health},
                                   Health{collision_damage_test::player_health}};
    data.runtime.collision.entity_bounds.set_half_extents(EntityType::PlayerShip,
                                                          {{1.f, 1.f, 1.f}});
    tests::add_capital_spawn(data,
                             Vector3f{},
                             Team::White,
                             60.f,
                             60.f,
                             ioj::settings::PositiveHealth::make(
                                 ioj::sim::Health{collision_damage_test::capital_health}));

    tests::WorldlessSimulationTest harness{std::move(data).compile()};

    auto& simulation{harness.simulation()};
    auto* const player{simulation.player_ship_simulation()};
    ASSERT_NE(player, nullptr) << "Collision test player simulation is available";

    auto const player_id{player->unique_entity_id};
    auto const capital_id{simulation.capital_ships().get_entities().entity_ids()[0]};
    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    for (std::int32_t index{}; index < 3; ++index) {
        SCOPED_TRACE(index);
        simulation.advance(dt);
        auto const capital{observe_entity(simulation, capital_id)};
        auto const damage{(index + 1) * collision_damage_test::overlap_damage};
        EXPECT_EQ(player->health().health.value(), collision_damage_test::player_health - damage);
        EXPECT_EQ(capital ? capital->health.value() : 0,
                  index == 2 ? 0 : collision_damage_test::capital_health - damage);
    }
    EXPECT_TRUE(entity_is_alive(simulation, player_id)) << "Player survives the third overlap tick";
    EXPECT_FALSE(entity_is_alive(simulation, capital_id))
        << "Capital death commits in the third overlap tick";
    EXPECT_FALSE(tests::has_recorded_kills(harness.ledger()))
        << "Collision death grants no combat kill";
    EXPECT_TRUE(
        harness.ledger().history().life_state()[harness.ledger().history_index(capital_id)] ==
        LifeState::Unknown)
        << "Collision death is environmental";
}
}
