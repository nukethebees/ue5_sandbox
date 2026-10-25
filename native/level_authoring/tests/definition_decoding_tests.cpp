#include "settings_diff.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/testing/math_diff.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>

namespace ioj::levels::authoring::decoding_tests {
namespace {
constexpr char valid_camera_level[]{R"(
(level
  :id 'camera-example
  :title "Camera Example"
  :camera (camera
    :look-at '(blue-capital red-capital)
    :distance 10000
    :offset-direction '(-1 -1 0.5))
  :entities (list
    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(-1000 0 0) :rotation '(0 0 0))
    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0))))
)"};
constexpr char scheduled_level[]{R"(
(level
  :id 'scheduled-example
  :title "Scheduled Example"
  :camera (camera :look-at '(hero) :distance 1000 :offset-direction '(-1 0 0))
  :mission (mission :objectives (list
    (objective :id 'primary :team 'blue :pass (entity-destroyed 'enemy))))
  :entities (list
    (entity :id 'hero :archetype 'capital-ship :team 'blue
      :position '(0 0 0) :rotation '(0 0 0))
    (entity :id 'enemy :archetype 'capital-ship :team 'red
      :position '(1000 0 0) :rotation '(0 180 0) :spawn-at 1.25)))
)"};
}

TEST(S7LevelDefinition, DecodesScheduledSpawnsAndMissionObjectives) {
    auto const result{DefinitionReader{}.read_level_source(scheduled_level)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->entities()[1].spawn_time_seconds, 1.25);
    EXPECT_EQ(std::get<EntityDestroyedExpr>(result->mission()->objectives[0].expressions[0].payload)
                  .entity,
              EntityId{"enemy"});
}

TEST(S7LevelDefinition, DecodesPlayerlessCameraDefinition) {
    auto const result{DefinitionReader{}.read_level_source(valid_camera_level)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_TRUE(result->player_entity_id().empty());
    EXPECT_FALSE(result->metadata().par_time_seconds);
    std::optional<LevelCameraDefinition> const expected{
        {{EntityId{"blue-capital"}, EntityId{"red-capital"}}, {-1., -1., .5}, 10000.}};
    EXPECT_PRED_FORMAT2(ioj::testing::structurally_equal, expected, result->camera());
    EXPECT_PRED_FORMAT2(ioj::testing::structurally_equal, settings::Settings{}, result->settings());
}

TEST(S7LevelDefinition, DecodesTimedMissionValues) {
    std::string source{scheduled_level};
    auto const expression{std::string_view{"(entity-destroyed 'enemy)"}};
    source.replace(source.find(expression), expression.size(), "(time-elapsed 45.5)");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(std::get<TimeElapsedExpr>(result->mission()->objectives[0].expressions[0].payload)
                  .seconds.get(),
              45.5);
}

TEST(S7LevelDefinition, DecodesDeclarativeUnlockCriteria) {
    std::string source{scheduled_level};
    source.insert(source.size() - 2,
                  " :unlock (list (level-completed 'first-level) (level-completed 'second-level))");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->unlock_level_ids(),
              (std::vector{LevelId{"first-level"}, LevelId{"second-level"}}));
}
}
