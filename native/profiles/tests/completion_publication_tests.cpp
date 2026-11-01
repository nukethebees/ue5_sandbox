#include <ioj/profiles/completion_publication.h>
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/profile_id.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/levels/reference_level_simulation_data.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <gtest/gtest.h>

namespace ioj::profiles {
TEST(CompletionPublication, PublishesRetainedPlayerResultAndPayloadWithoutUnreal) {
    levels::ObjectiveDefinition objective{.id = levels::ObjectiveName{"primary"},
                                          .team = Team::Blue,
                                          .wins_level = ObjectiveWinsLevel::Yes};
    objective.expressions = {
        {levels::TimeElapsedExpr{.seconds = NonNegativeFinite<double>::make(0.2)}}};
    objective.roots[ObjectiveOutcome::Passed] = 0;
    auto const definition{levels::LevelDefinition::make(
        {.metadata = {.id = levels::LevelId{"publication"},
                      .title = "Publication",
                      .par_time_seconds = 1.f},
         .player_entity_id = levels::EntityId{"player"},
         .mission = levels::MissionDefinition{.objectives = {std::move(objective)}},
         .entities = {{.id = levels::EntityId{"player"},
                       .archetype = levels::EntityArchetype::PlayerFighter,
                       .team = Team::Blue,
                       .position = {0., 30000., 0.}},
                      {.id = levels::EntityId{"capital"},
                       .archetype = levels::EntityArchetype::CapitalShip,
                       .team = Team::Red},
                      {.id = levels::EntityId{"turret"},
                       .archetype = levels::EntityArchetype::StaticTurret,
                       .team = Team::Red,
                       .position = {20000., 0., 0.}}}})};
    ASSERT_TRUE(definition);
    auto geometry{sim::make_reference_level_simulation_data()};
    geometry.data.player = geometry.player;
    geometry.data.collision.grid_geometry.dimensions = {16, 16, 4};
    geometry.data.collision.grid_geometry.cell_dimensions = {10000.f, 10000.f, 10000.f};
    geometry.data.frame_memory_capacity_bytes = 4 * 1024 * 1024;
    geometry.data.clock_settings.tick_rate = Frequency::make(10.);
    auto built{sim::make_level_simulation_init_data(*definition, std::move(geometry.data))};
    ASSERT_TRUE(built) << built.error().front();
    sim::LevelSim simulation{*std::pmr::new_delete_resource(), std::move(*built)};
    simulation.finish_initialisation();
    EXPECT_FALSE(publish_player_completion(simulation, nullptr, 17));

    auto const player{simulation.get_player_ship_simulation()->unique_entity_id};
    sim::DirectDamageEvents damage;
    damage.add(simulation.get_capital_ships().get_id(0), 100000, player);
    damage.add(simulation.get_turrets().get_entities().entity_ids()[0], 100000, player);
    sim::LevelSimTestAccess::queue_direct_damage_events(simulation, damage.get_const_view());
    simulation.start();
    simulation.advance(0.4);
    auto const root{std::filesystem::temp_directory_path() / ("publication-" + make_profile_id())};
    ProfileManager profiles{make_file_profile_storage(root)};
    ASSERT_TRUE(profiles.initialise());
    auto const publication{publish_player_completion(simulation, &profiles, 17)};
    ASSERT_TRUE(publication);
    EXPECT_EQ(publication->level_title, "Publication");
    EXPECT_EQ(publication->par_time_seconds, 1.f);
    ScoreRecord const expected{
        .date = 17,
        .level_name = "publication",
        .team = Team::Blue,
        .outcome = MissionOutcome::Victory,
        .objective_id = "primary",
        .objective_outcome = ObjectiveOutcome::Passed,
        .kills = 2,
        .time_seconds = static_cast<float>(0.2 + simulation.get_clock().get_tick_period())};
    EXPECT_EQ(publication->record, expected);
    EXPECT_TRUE(publication->save_result.persisted);
    EXPECT_TRUE(publication->save_result.new_best_time);
    ASSERT_EQ(profiles.get_active_records().size(), 1u);
    EXPECT_EQ(profiles.get_active_records()[0], expected);
    EXPECT_TRUE(simulation.get_player_team_completion());
    EXPECT_FALSE(simulation.get_mission().completions().empty());
    EXPECT_EQ(profiles.get_active_records().size(), 1u);
    std::filesystem::remove_all(root);
    auto lock_path{root};
    lock_path += ".lock";
    std::filesystem::remove(lock_path);
}
}
