#include "support/simulation_test_support.h"
#include <ioj/presentation/hud_model.h>
#include <ioj/sim/missions/mission.h>

namespace ioj::sim::tests {
namespace hud_test {
auto data() -> LevelSimInitData {
    auto result{make_simulation_data(make_fixture())};
    result.capital_ships.fighter_spawn_slots = 0;
    result.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    return result;
}
auto defence_mission(LevelSimInitData const& data) -> Mission {
    levels::MissionDefinition definition{
        .objectives = {
            {.id = levels::ObjectiveName{"defence"},
             .title = "Hold position",
             .team = Team::Blue,
             .expressions = {{levels::TimeElapsedExpr{NonNegativeFinite<double>{0.2}}},
                             {levels::EntityDestroyedExpr{levels::EntityId{"defended"}}}},
             .roots = levels::ObjectiveRoots{
                 std::array<levels::ExprIndex, 3>{0, 1, levels::absent_expression}}}}};
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{Mission::compile(
        definition, std::array{levels::EntityId{"defended"}, levels::EntityId{"enemy"}}, clock)};
    assert(compiled);
    return std::move(*compiled);
}
}

TEST(HudModel, InitialCachesAndChangeDetectionNeedNoWidget) {
    auto data{hud_test::data()};
    add_capital_spawn(data, {0.f, 0.f, 0.f}, Team::Blue, 60.f);
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& simulation{harness.simulation()};
    presentation::HudModel hud;
    hud.initialise(simulation.mission(), simulation.read_access());

    EXPECT_TRUE(hud.active());
    EXPECT_FALSE(hud.has_mission_data());
    EXPECT_FALSE(hud.player_status_data());
    EXPECT_FALSE(hud.player_flight_data());
    EXPECT_FALSE(hud.sampled_speed_view());
    auto const& counts{hud.entity_count_data().alive_per_team_and_type};
    EXPECT_EQ(counts[std::to_underlying(Team::Blue)][std::to_underlying(EntityType::CapitalShip)],
              1);
    EXPECT_EQ(counts[std::to_underlying(Team::Red)][std::to_underlying(EntityType::CapitalShip)],
              1);
    auto const changes{hud.force_sample()};
    EXPECT_FALSE(changes.mission);
    EXPECT_FALSE(changes.entity_counts);
    EXPECT_FALSE(changes.player_status);
    EXPECT_FALSE(changes.player_flight);

    hud.deactivate();
    EXPECT_FALSE(hud.active());
    EXPECT_FALSE(hud.sampled_speed_view());
    EXPECT_FALSE(hud.tick(1).entity_counts);
}

TEST(HudModel, EntityCountsPollAtConfiguredPeriodWithoutWidgets) {
    auto data{hud_test::data()};
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};
    presentation::HudModel hud;
    hud.initialise(simulation.mission(), simulation.read_access());
    auto const victim{simulation.capital_ships().get_entities().entity_ids()[0]};
    harness.queue_kills(std::array{victim});
    simulation.start();
    harness.advance(0.05);

    auto const team{std::to_underlying(Team::Red)};
    auto const type{std::to_underlying(EntityType::CapitalShip)};
    EXPECT_FALSE(hud.tick(14).entity_counts);
    EXPECT_EQ(hud.entity_count_data().alive_per_team_and_type[team][type], 1);
    EXPECT_TRUE(hud.tick(1).entity_counts);
    EXPECT_EQ(hud.entity_count_data().alive_per_team_and_type[team][type], 0);
}

TEST(HudModel, CachesDefenceFailureAndObjectiveMembership) {
    auto data{hud_test::data()};
    add_capital_spawn(data, {}, Team::Blue, 60.f);
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    data.mission = hud_test::defence_mission(data);
    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};
    presentation::HudModel hud;
    hud.initialise(simulation.mission(), simulation.read_access());
    EXPECT_TRUE(hud.has_mission_data());
    EXPECT_EQ(hud.mission_data().active, 0);
    EXPECT_TRUE(hud.mission_data().active_titles.empty());

    simulation.start();
    harness.advance(simulation.clock().tick_period());
    EXPECT_TRUE(hud.force_sample().mission);
    EXPECT_EQ(hud.mission_data().active, 1);
    EXPECT_EQ(hud.mission_data().active_titles, "Blue: Hold position");

    harness.queue_kills(std::array{simulation.capital_ships().get_entities().entity_ids()[0]});
    harness.advance(0.05);
    EXPECT_TRUE(hud.force_sample().mission);
    EXPECT_EQ(hud.mission_data().completed, 1);
    EXPECT_EQ(hud.mission_data().active, 0);
    EXPECT_TRUE(hud.mission_data().active_titles.empty());
    EXPECT_EQ(simulation.mission().outcome(ObjectiveId{0}), ObjectiveOutcome::Failed);
}

TEST(HudModel, PlayerStatusKillRankingAndMatrixRetainPayloads) {
    auto const fixture{make_fixture()};
    auto data{hud_test::data()};
    auto player{make_player_spawn(fixture)};
    player.team = Team::Blue;
    player.health = {Health{37}, Health{100}};
    add_player_spawn(data, player);
    for (std::int32_t index{}; index < 3; ++index) {
        add_capital_spawn(
            data, {20000.f + 10000.f * static_cast<float>(index), 0.f, 0.f}, Team::Red, 60.f);
    }
    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};
    auto const player_id{simulation.player_ship_simulation()->unique_entity_id};
    auto const& capitals{simulation.capital_ships()};
    auto const rival{capitals.get_entities().entity_ids()[2]};
    presentation::HudModel hud;
    hud.initialise(simulation.mission(), simulation.read_access());
    ASSERT_TRUE(hud.player_status_data());
    EXPECT_EQ(hud.player_status_data()->health.health.value(), 37);
    EXPECT_EQ(hud.player_status_data()->health.max_health.value(), 100);
    {
        auto const samples{hud.sampled_speed_view()};
        ASSERT_TRUE(samples);
        auto const* player_simulation{simulation.player_ship_simulation()};
        EXPECT_EQ(samples->samples.data(), player_simulation->speed_samples.data());
        EXPECT_EQ(samples->samples.size(), player_simulation->speed_samples.size());
        EXPECT_EQ(samples->oldest_index, 0);
    }
    auto commands{simulation.player_ship_commands()};
    ASSERT_TRUE(commands);
    commands->set_speed_sampling_enabled(true);
    commands->set_forward_input(1.f);
    harness.queue_kills(std::array{capitals.get_entities().entity_ids()[0]}, player_id);
    harness.queue_kills(std::array{capitals.get_entities().entity_ids()[1]}, rival);
    simulation.start();
    harness.advance(0.05);
    hud.set_selected_mapping_context("Flight");
    hud.force_sample();

    ASSERT_TRUE(hud.player_status_data());
    EXPECT_EQ(hud.player_status_data()->points, 1);
    ASSERT_TRUE(hud.player_flight_data());
    EXPECT_EQ(hud.player_flight_data()->selected_mapping_context, "Flight");
    {
        auto const samples{hud.sampled_speed_view()};
        ASSERT_TRUE(samples);
        auto const* player_simulation{simulation.player_ship_simulation()};
        EXPECT_EQ(samples->samples.data(), player_simulation->speed_samples.data());
        EXPECT_EQ(samples->oldest_index, player_simulation->speed_sample_index);
        ASSERT_GT(samples->oldest_index, 0);
        EXPECT_GT(samples->samples[samples->oldest_index - 1].x(), 0.0);
        EXPECT_GT(samples->samples[samples->oldest_index - 1].y(), 0.0);
    }
    auto const& kills{hud.kill_data()};
    ASSERT_EQ(kills.top_killers.size(), 2);
    EXPECT_EQ(kills.top_killers[0].entity_id, player_id);
    EXPECT_EQ(kills.top_killers[0].entity_type, EntityType::PlayerShip);
    EXPECT_EQ(kills.top_killers[0].team, Team::Blue);
    EXPECT_EQ(kills.top_killers[0].kills, 1);
    EXPECT_EQ(kills.top_killers[1].entity_id, rival);
    EXPECT_EQ(kills.team_kill_matrix[std::to_underlying(Team::Blue)]
                                    [std::to_underlying(EntityType::CapitalShip)],
              1);
    EXPECT_EQ(kills.team_kill_matrix[std::to_underlying(Team::Red)]
                                    [std::to_underlying(EntityType::CapitalShip)],
              1);
    harness.queue_kills(std::array{player_id});
    harness.advance(0.05);
    hud.force_sample();
    auto const* const dead_player{simulation.player_ship_simulation()};
    ASSERT_NE(dead_player, nullptr);
    EXPECT_EQ(dead_player->health().health.value(), 0);
    EXPECT_EQ(simulation.entity_ledger().count_alive(EntityType::PlayerShip), 0);
    ASSERT_TRUE(hud.player_status_data());
    EXPECT_EQ(hud.player_status_data()->health.health.value(), 0);
    EXPECT_TRUE(hud.player_flight_data());
    {
        auto const samples{hud.sampled_speed_view()};
        ASSERT_TRUE(samples);
        EXPECT_EQ(samples->samples.data(), dead_player->speed_samples.data());
    }
    hud.deactivate();
    EXPECT_FALSE(hud.player_status_data());
    EXPECT_FALSE(hud.player_flight_data());
    EXPECT_FALSE(hud.sampled_speed_view());
}

TEST(HudModel, MissionDisplayTimeUsesSimulationClock) {
    auto data{hud_test::data()};
    data.clock_settings.tick_rate = Frequency{10.0};
    data.clock_settings.time_scale = PositiveFinite<double>{2.0};
    add_capital_spawn(data, {}, Team::Blue, 60.f);
    add_capital_spawn(data, {20000.f, 0.f, 0.f}, Team::Red, 60.f);
    data.mission = hud_test::defence_mission(data);
    WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};
    presentation::HudModel hud;
    hud.initialise(simulation.mission(), simulation.read_access());
    hud.advance(1.0);
    EXPECT_EQ(hud.mission_data().completed, 0);
    EXPECT_DOUBLE_EQ(hud.mission_data().elapsed_seconds, 0.0);

    simulation.start();
    harness.advance(0.05);
    hud.force_sample();
    EXPECT_DOUBLE_EQ(hud.mission_data().elapsed_seconds, 0.1);
    EXPECT_EQ(hud.mission_data().completed, 0);
    harness.advance(0.05);
    hud.force_sample();
    EXPECT_DOUBLE_EQ(hud.mission_data().elapsed_seconds, 0.2);
    EXPECT_EQ(hud.mission_data().completed, 0);
    harness.advance(0.05);
    hud.force_sample();
    EXPECT_DOUBLE_EQ(hud.mission_data().elapsed_seconds, 0.3);
    EXPECT_EQ(hud.mission_data().completed, 1);
    EXPECT_EQ(simulation.mission().outcome(ObjectiveId{0}), ObjectiveOutcome::Passed);
}
}
