#include "support/simulation_test_support.h"
#include <ioj/bridge/level_frame.h>
#include <ioj/presentation/hud_model.h>
#include <ioj/sim/levels/level_compilation.h>

namespace ioj::sim::tests {
namespace hud_test {
auto data() -> SimulationScenario {
    SimulationScenario result{make_simulation_data(make_fixture())};
    result.runtime.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    return result;
}
}

TEST(HudModel, InitialCachesAndChangeDetectionNeedNoWidget) {
    auto data{hud_test::data()};
    add_capital_spawn(data, {0.f, 0.f, 0.f}, Team::Blue, 60.f);
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    WorldlessSimulationTest harness{std::move(data).compile()};

    auto const& simulation{harness.simulation()};
    presentation::HudModel hud;
    auto const frame = [&] { return bridge::level_frame(simulation); };
    auto const initial{frame()};
    hud.initialise(initial.entity_counts, initial.player);

    EXPECT_TRUE(hud.active());
    EXPECT_FALSE(hud.player_data());
    EXPECT_FALSE(frame().player);
    auto const& counts{hud.entity_count_data()};
    EXPECT_EQ((counts[Team::Blue, EntityType::CapitalShip]), 1);
    EXPECT_EQ((counts[Team::Red, EntityType::CapitalShip]), 1);
    auto const changes{hud.force_sample(initial.entity_counts, initial.player)};
    EXPECT_FALSE(changes.entity_counts);
    EXPECT_FALSE(changes.player_status);
    EXPECT_FALSE(changes.player_flight);

    hud.deactivate();
    EXPECT_FALSE(hud.active());
    EXPECT_FALSE(frame().player);
    EXPECT_FALSE(hud.tick(1, initial.entity_counts, initial.player).entity_counts);
}

TEST(HudModel, EntityCountsPollAtConfiguredPeriodWithoutWidgets) {
    auto data{hud_test::data()};
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    WorldlessSimulationTest harness{std::move(data).compile()};

    auto& simulation{harness.simulation()};
    presentation::HudModel hud;
    auto const frame = [&] { return bridge::level_frame(simulation); };
    auto const initial{frame()};
    hud.initialise(initial.entity_counts, initial.player);
    auto const victim{simulation.capital_ships().get_entities().entity_ids()[0]};
    harness.queue_kills(std::array{victim});
    simulation.start();
    harness.advance(0.05);

    auto const team{Team::Red};
    auto const type{EntityType::CapitalShip};
    auto const updated{frame()};
    EXPECT_FALSE(hud.tick(14, updated.entity_counts, updated.player).entity_counts);
    EXPECT_EQ((hud.entity_count_data()[team, type]), 1);
    EXPECT_TRUE(hud.tick(1, updated.entity_counts, updated.player).entity_counts);
    EXPECT_EQ((hud.entity_count_data()[team, type]), 0);
}

TEST(HudModel, PlayerStatusAndFlightRetainPayloads) {
    auto const fixture{make_fixture()};
    auto data{hud_test::data()};
    auto player{make_player_spawn(fixture)};
    player.team = Team::Blue;
    player.sample_speed = true;
    player.health = {Health{37}, Health{100}};
    add_player_spawn(data, player);
    for (std::int32_t index{}; index < 3; ++index) {
        add_capital_spawn(
            data, {20000.f + 10000.f * static_cast<float>(index), 0.f, 0.f}, Team::Red, 60.f);
    }

    WorldlessSimulationTest harness{std::move(data).compile()};

    auto& simulation{harness.simulation()};
    auto const player_id{simulation.player_ship_simulation()->unique_entity_id};
    auto const& capitals{simulation.capital_ships()};
    auto const rival{capitals.get_entities().entity_ids()[2]};
    presentation::HudModel hud;
    auto const frame = [&] { return bridge::level_frame(simulation); };
    auto const initial{frame()};
    hud.initialise(initial.entity_counts, initial.player);
    ASSERT_TRUE(hud.player_data());
    EXPECT_EQ(hud.player_data()->status.health.health.value(), 37);
    EXPECT_EQ(hud.player_data()->status.health.max_health.value(), 100);
    {
        auto const samples{frame().player->sampled_speed};
        auto const* player_simulation{simulation.player_ship_simulation()};
        EXPECT_EQ(samples.samples.data(), player_simulation->speed_samples.data());
        EXPECT_EQ(samples.samples.size(), player_simulation->speed_samples.size());
        EXPECT_EQ(samples.oldest_index, 0);
    }

    harness.queue_kills(std::array{capitals.get_entities().entity_ids()[0]}, player_id);
    harness.queue_kills(std::array{capitals.get_entities().entity_ids()[1]}, rival);
    simulation.start();
    harness.advance(0.05);
    auto const updated{frame()};
    hud.force_sample(updated.entity_counts, updated.player);

    ASSERT_TRUE(hud.player_data());
    EXPECT_EQ(hud.player_data()->status.points, 1);
    {
        auto const samples{frame().player->sampled_speed};
        auto const* player_simulation{simulation.player_ship_simulation()};
        EXPECT_EQ(samples.samples.data(), player_simulation->speed_samples.data());
        EXPECT_EQ(samples.oldest_index, player_simulation->speed_sample_index);
        ASSERT_GT(samples.oldest_index, 0);
        EXPECT_GT(samples.samples[samples.oldest_index - 1].x(), 0.0);
        EXPECT_GT(samples.samples[samples.oldest_index - 1].y(), 0.0);
    }
}

}
