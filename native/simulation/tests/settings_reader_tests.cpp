#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/s7/simulation_settings_reader.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <format>

namespace ioj::sim::tests {
using levels::authoring::DefinitionReader;

TEST(SimulationSettingsReader, AppliesSparseNestedLayersIncludingExplicitDefaults) {
    SimulationSettings const defaults;
    auto const source{
        DefinitionReader{}.read_settings_source(std::format(R"(
      (settings-layers
        (settings :fighters (fighters :speed 9000 :laser (laser :damage 17 :max-distance 50000))
                  :turrets (turrets :laser (laser :damage 23)))
        (settings :fighters (fighters :speed {} :laser (laser :damage 31)))))",
                                                            defaults.fighters.speed))};
    ASSERT_TRUE(source);
    EXPECT_EQ(source->size(), 6);
    auto const resolved{s7::resolve_simulation_settings(*source)};
    ASSERT_TRUE(resolved) << resolved.error().front().message;
    EXPECT_EQ(resolved->fighters.speed, defaults.fighters.speed);
    EXPECT_EQ(resolved->fighters.laser.damage, 31);
    EXPECT_EQ(resolved->fighters.laser.max_distance, 50000);
    EXPECT_EQ(resolved->fighters.laser.projectile_speed, defaults.fighters.laser.projectile_speed);
    EXPECT_EQ(resolved->turrets.laser.damage, 23);
    EXPECT_EQ(resolved->player.laser.damage, defaults.player.laser.damage);
}

TEST(SimulationSettingsReader, PreservesBaseFileCompatibilityAndLevelMission) {
    auto const root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"};
    auto const base{s7::read_simulation_settings(root / "Settings/base.scm", root)};
    ASSERT_TRUE(base);
    EXPECT_EQ(base->fighters.speed, 4500);
    EXPECT_EQ(base->fighters.laser.damage, 10);
    EXPECT_EQ(base->spinners.laser.damage, 2);

    auto const level{DefinitionReader{}.read_level_source(R"(
      (level :id 'settings-mission :title "Settings and mission" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings (settings :fighters (fighters :laser (laser :damage 77)))
        :mission (mission :objectives (list
          (objective :id 'survive :team 'blue :pass (time-elapsed 5))))))")};
    ASSERT_TRUE(level) << level.error().front().message;
    auto const mission{level->mission()};
    auto const resolved{s7::read_simulation_settings(root / "Settings/base.scm", root, &*level)};
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->fighters.laser.damage, 77);
    EXPECT_EQ(resolved->fighters.speed, base->fighters.speed);
    EXPECT_EQ(level->mission(), mission);
}

TEST(SimulationSettingsReader, ConstructsAttackBandAfterAllLayersAndLevelOverrides) {
    auto const source{DefinitionReader{}.read_settings_source(R"(
      (settings-layers
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :minimum-ratio 0.8 :desired-ratio 0.9 :maximum-ratio 1)))
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :desired-ratio 0.85)))))")};
    ASSERT_TRUE(source);
    auto const resolved{s7::resolve_simulation_settings(*source)};
    ASSERT_TRUE(resolved) << resolved.error().front().message;
    EXPECT_EQ(resolved->fighters.attack_distance_band, *AttackDistanceBand::make(0.8f, 0.85f, 1.f));

    auto const level{DefinitionReader{}.read_level_source(R"(
      (level :id 'band :title "Band" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :minimum-ratio 0 :desired-ratio 0 :maximum-ratio 0)))))")};
    ASSERT_TRUE(level) << level.error().front().message;
    auto const overridden{s7::resolve_simulation_settings(*source, &*level)};
    ASSERT_TRUE(overridden) << overridden.error().front().message;
    EXPECT_EQ(overridden->fighters.attack_distance_band, *AttackDistanceBand::make(0.f, 0.f, 0.f));
}

TEST(SimulationSettingsReader, RejectsInvalidFinalAttackBandWithoutExposingMutableRuntimeFields) {
    auto const source{DefinitionReader{}.read_settings_source(R"(
      (settings :fighters (fighters :attack-distance-band
        (attack-distance-band :minimum-ratio 0.9))))")};
    ASSERT_TRUE(source);
    auto const resolved{s7::resolve_simulation_settings(*source)};
    ASSERT_FALSE(resolved);
    ASSERT_EQ(resolved.error().size(), 1u);
    EXPECT_EQ(resolved.error().front().code, levels::DiagnosticCode::InvalidNumber);
    EXPECT_EQ(resolved.error().front().node_path, "fighters.attack-distance-band");
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
          Case{"(settings :fighters (fighters :speed 1e100))", InvalidType, "fighters.speed"},
          Case{"(settings :fighters (fighters :health 1.5))", InvalidType, "fighters.health"},
          Case{"(settings :fighters (fighters :health -1))", InvalidNumber, "fighters.health"},
          Case{"(settings :tick (tick :time-scale 0))", InvalidNumber, "tick.time-scale"}}) {
        SCOPED_TRACE(test.source);
        auto const source{DefinitionReader{}.read_settings_source(test.source)};
        ASSERT_TRUE(source);
        auto const resolved{s7::resolve_simulation_settings(*source)};
        ASSERT_FALSE(resolved);
        ASSERT_FALSE(resolved.error().empty());
        EXPECT_EQ(resolved.error().front().code, test.code);
        EXPECT_EQ(resolved.error().front().node_path, test.path);
    }
}
}
