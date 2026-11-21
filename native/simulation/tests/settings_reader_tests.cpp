#include "../../level_authoring/tests/module_fixture.h"
#include "../../level_authoring/tests/settings_diff.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <format>

namespace ioj::sim::tests {
using ioj::testing::structurally_equal;
using levels::authoring::DefinitionReader;

TEST(SimulationSettingsReader, AppliesSparseNestedLayersIncludingExplicitDefaults) {
    settings::Settings const defaults;
    auto const source{
        ioj::levels::authoring::tests::read_test_settings(DefinitionReader{},
                                                          std::format(R"(
      (settings-layers
        (settings :fighters (fighters :speed 9000 :laser (laser :damage 17 :max-distance 50000))
                  :turrets (turrets :laser (laser :damage 23)))
        (settings :fighters (fighters :speed {} :laser (laser :damage 31)))))",
                                                                      defaults.fighters.speed))};
    ASSERT_TRUE(source);
    settings::Settings expected;
    expected.fighters.laser.damage = 31;
    expected.fighters.laser.max_distance = ::ioj::PositiveFinite<float>::make(50000);
    expected.turrets.laser.damage = 23;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, *source);
    EXPECT_TRUE(validate_settings(*source));
}

TEST(SimulationSettingsReader, PreservesBaseFileCompatibilityAndLevelLogic) {
    auto const root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"};
    auto const base{
        DefinitionReader{root}.read_settings_file(root / "Settings/level-defaults.scm")};
    ASSERT_TRUE(base);
    EXPECT_EQ(base->fighters.speed, 4500);
    EXPECT_EQ(base->fighters.laser.damage, 10);
    EXPECT_EQ(base->tube_spinners.laser.damage, 2);

    auto const base_values{
        DefinitionReader{root}.read_settings_file(root / "Settings/level-defaults.scm")};
    ASSERT_TRUE(base_values);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) -- Independent mutation snapshot.
    auto const original{*base_values};
    auto const level{ioj::levels::authoring::tests::read_test_level(DefinitionReader{root}, R"(
      (level :id 'settings-logic :title "Settings and logic" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings (settings-layers (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
                    (settings :fighters (fighters :laser (laser :damage 77))))
        :logic (logic
          (define delay (timer))
          (initially (start-timer delay 5))
          (on :when (timer-elapsed delay) :do (win 'blue)))))")};
    ASSERT_TRUE(level) << level.error().front().message;
    auto const logic{level->logic()};
    ASSERT_TRUE(validate_settings(level->settings()));
    auto expected{*base_values};
    expected.fighters.laser.damage = 77;
    EXPECT_PRED_FORMAT2(structurally_equal, expected, level->settings());
    EXPECT_EQ(level->logic(), logic);

    for (auto const field : {"", ":settings (settings)", ":settings (settings-layers)"}) {
        SCOPED_TRACE(field);
        auto const inherited{ioj::levels::authoring::tests::read_test_level(DefinitionReader{},
                                                                            std::format(R"(
          (level :id 'inherited :title "Inherited" :player 'player {}
            :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                                   :position '(0 0 0)))))",
                                                                                        field))};
        ASSERT_TRUE(inherited) << levels::format_diagnostics(inherited.error());
        EXPECT_PRED_FORMAT2(structurally_equal, settings::Settings{}, inherited->settings());
    }
    EXPECT_FALSE(ioj::levels::authoring::tests::read_test_level(DefinitionReader{},
                                                                "(level :title \"Missing id\")"));
    EXPECT_FALSE(ioj::levels::authoring::tests::read_test_settings(
        DefinitionReader{},
        "(settings :fighters (fighters :speed 1 :laser (laser :damage 'invalid)))",
        *base_values));
    EXPECT_PRED_FORMAT2(structurally_equal, original, *base_values);
}

TEST(SimulationSettingsReader, ResolvesCollisionGridThroughOrdinaryLevelSettings) {
    settings::Settings base;
    base.fighters.speed = 7300.f;
    base.collision_grid.level_size = {80000.f, 60000.f, 40000.f};
    base.collision_grid.damage_per_overlap_detection = ioj::Positive<std::int32_t>{75};
    auto const original{base};
    auto const level{ioj::levels::authoring::tests::read_test_level(DefinitionReader{}, R"(
      (level :id 'grid :title "Grid" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings (settings-layers
          (settings :fighters (fighters :speed 7300)
            :collision-grid (collision-grid :level-size '(80000 60000 40000)
                                           :damage-per-overlap-detection 75))
          (settings :collision-grid (collision-grid :cell-size '(1000 1000 1000)))
          (settings :collision-grid (collision-grid :cell-size '(5000 5000 20000))))))")};
    ASSERT_TRUE(level) << levels::format_diagnostics(level.error());
    EXPECT_PRED_FORMAT2(structurally_equal, original, level->settings());
    EXPECT_PRED_FORMAT2(structurally_equal, original, base);
    ASSERT_TRUE(validate_settings(level->settings()));

    auto const legacy{ioj::levels::authoring::tests::read_test_level(DefinitionReader{}, R"(
      (level :id 'grid :title "Grid" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :collision-grid (collision-grid :cell-size '(5000 5000 20000))))")};
    ASSERT_FALSE(legacy);
    EXPECT_EQ(legacy.error().front().code, levels::DiagnosticCode::UnknownProperty);
}

TEST(SimulationSettingsReader, RetainsUnknownFieldConversionAndFinalValidationErrors) {
    struct Case {
        std::string_view source;
        levels::DiagnosticCode code;
        std::string_view path;
    };
    using enum levels::DiagnosticCode;
    for (auto const& test :
         {Case{"(settings :fighters (fighters :fire-point-distance 1))",
               UnknownProperty,
               "fighters.fire-point-distance"},
          Case{"(settings :fighters (fighters :speed 'fast))", InvalidType, "fighters.speed"},
          Case{"(settings :fighters (fighters :speed 1e100))", InvalidNumber, "fighters.speed"},
          Case{"(settings :fighters (fighters :health 1.5))", InvalidNumber, "fighters.health"},
          Case{"(settings :fighters (fighters :health -1))", InvalidType, "fighters.health"},
          Case{"(settings :collision-grid (collision-grid :damage-per-overlap-detection 0))",
               InvalidNumber,
               "collision-grid.damage-per-overlap-detection"},
          Case{"(settings :collision-grid (collision-grid :damage-per-overlap-detection -1))",
               InvalidNumber,
               "collision-grid.damage-per-overlap-detection"},
          Case{"(settings :tick (tick :time-scale 0))", InvalidNumber, "tick.time-scale"},
          Case{"(settings :tick (tick :tick-rate 0))", InvalidNumber, "tick.tick-rate"},
          Case{"(settings :tick (tick :tick-rate -1))", InvalidNumber, "tick.tick-rate"}}) {
        SCOPED_TRACE(test.source);
        auto const source{
            ioj::levels::authoring::tests::read_test_settings(DefinitionReader{}, test.source)};
        auto const resolved{source.and_then(sim::validate_settings)};
        ASSERT_FALSE(resolved);
        ASSERT_FALSE(resolved.error().empty());
        EXPECT_EQ(resolved.error().front().code, test.code);
        EXPECT_EQ(resolved.error().front().node_path, test.path);
    }
}
}
