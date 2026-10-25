#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/s7/simulation_settings_reader.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <format>

namespace ioj::sim::tests {
using levels::authoring::DefinitionReader;
using settings::AttackDistanceBand;

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
    EXPECT_EQ(source->player_ship.health, defaults.player.health);
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

    auto const base_values{DefinitionReader{root}.read_settings_file(root / "Settings/base.scm")};
    ASSERT_TRUE(base_values);
    auto const level{DefinitionReader{}.read_level_source(R"(
      (level :id 'settings-mission :title "Settings and mission" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings (settings :fighters (fighters :laser (laser :damage 77)))
        :mission (mission :objectives (list
          (objective :id 'survive :team 'blue :pass (time-elapsed 5))))))",
                                                          *base_values)};
    ASSERT_TRUE(level) << level.error().front().message;
    auto const mission{level->mission()};
    auto const resolved{s7::resolve_simulation_settings(*level->settings())};
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->fighters.laser.damage, 77);
    EXPECT_EQ(resolved->fighters.speed, base->fighters.speed);
    EXPECT_EQ(level->mission(), mission);

    for (auto const field : {"", ":settings (settings)", ":settings (settings-layers)"}) {
        SCOPED_TRACE(field);
        auto const inherited{DefinitionReader{}.read_level_source(std::format(R"(
          (level :id 'inherited :title "Inherited" :player 'player {}
            :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                                   :position '(0 0 0)))))",
                                                                              field),
                                                                  *base_values)};
        ASSERT_TRUE(inherited) << levels::format_diagnostics(inherited.error());
        ASSERT_TRUE(inherited->settings());
        EXPECT_EQ(inherited->settings()->fighters.speed, base->fighters.speed);
        EXPECT_EQ(inherited->settings()->fighters.laser.damage, base->fighters.laser.damage);
    }
    EXPECT_FALSE(
        DefinitionReader{}.read_level_source("(level :title \"Missing id\")", *base_values));
    EXPECT_FALSE(DefinitionReader{}.read_settings_source(
        "(settings :fighters (fighters :speed 1 :laser (laser :damage 'invalid)))", *base_values));
    EXPECT_EQ(base_values->fighters.speed, 4500);
    EXPECT_EQ(base_values->fighters.laser.damage, 10);
}

TEST(SimulationSettingsReader, ReplacesWholeAttackBandsAcrossLayersAndLevelOverrides) {
    auto const source{DefinitionReader{}.read_settings_source(R"(
      (settings-layers
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :minimum-ratio 0.8 :desired-ratio 0.9 :maximum-ratio 1)))
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :minimum-ratio 0.8 :desired-ratio 0.85 :maximum-ratio 1)))
        (settings :fighters (fighters :speed 5000))))")};
    ASSERT_TRUE(source);
    auto const resolved{s7::resolve_simulation_settings(*source)};
    ASSERT_TRUE(resolved) << resolved.error().front().message;
    EXPECT_EQ(resolved->fighters.attack_distance_band,
              *settings::AttackDistanceBand::try_make({0.8f, 0.85f, 1.f}));

    auto const level{DefinitionReader{}.read_level_source(R"(
      (level :id 'band :title "Band" :player 'player
        :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
                               :position '(0 0 0)))
        :settings
        (settings :fighters (fighters :attack-distance-band
          (attack-distance-band :minimum-ratio 0 :desired-ratio 0 :maximum-ratio 0)))))",
                                                          *source)};
    ASSERT_TRUE(level) << level.error().front().message;
    auto const overridden{s7::resolve_simulation_settings(*level->settings())};
    ASSERT_TRUE(overridden) << overridden.error().front().message;
    EXPECT_EQ(overridden->fighters.speed, 5000);
    EXPECT_EQ(overridden->fighters.attack_distance_band,
              *settings::AttackDistanceBand::try_make({0.f, 0.f, 0.f}));
}

TEST(SimulationSettingsReader, RejectsInvalidAttackBandsDuringDecoding) {
    for (auto const fields : {":minimum-ratio 0.9 :desired-ratio 0.5 :maximum-ratio 1",
                              ":minimum-ratio -0.1 :desired-ratio 0.5 :maximum-ratio 1",
                              ":minimum-ratio 0 :desired-ratio 0.5 :maximum-ratio 1.1",
                              ":minimum-ratio 0 :desired-ratio +nan.0 :maximum-ratio 1"}) {
        SCOPED_TRACE(fields);
        auto const source{DefinitionReader{}.read_settings_source(std::format(R"(
          (settings-layers
            (settings :fighters (fighters :attack-distance-band (attack-distance-band {})))
            (settings :fighters (fighters :attack-distance-band
              (attack-distance-band :minimum-ratio 0 :desired-ratio 0 :maximum-ratio 0)))))",
                                                                              fields))};
        ASSERT_FALSE(source);
        ASSERT_FALSE(source.error().empty());
        EXPECT_EQ(source.error().front().node_path, "fighters.attack-distance-band");
        EXPECT_EQ(source.error().front().message,
                  "Expected 0 <= minimum <= desired <= maximum <= 1");
    }

    auto const incomplete{DefinitionReader{}.read_settings_source(R"(
      (settings :fighters (fighters :attack-distance-band
        (attack-distance-band :minimum-ratio 0.9))))")};
    ASSERT_FALSE(incomplete);
    ASSERT_EQ(incomplete.error().size(), 2u);
    EXPECT_EQ(incomplete.error().front().code, levels::DiagnosticCode::MissingProperty);
    EXPECT_EQ(incomplete.error().front().node_path, "fighters.attack-distance-band.desired-ratio");
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
          Case{"(settings :fighters (fighters :health -1))", InvalidNumber, "fighters.health"},
          Case{"(settings :tick (tick :time-scale 0))", InvalidNumber, "tick.time-scale"},
          Case{"(settings :tick (tick :tick-rate 0))", InvalidNumber, "tick.tick-rate"},
          Case{"(settings :tick (tick :tick-rate -1))", InvalidNumber, "tick.tick-rate"}}) {
        SCOPED_TRACE(test.source);
        auto const source{DefinitionReader{}.read_settings_source(test.source)};
        auto const resolved{source
                                ? s7::resolve_simulation_settings(*source)
                                : s7::SimulationSettingsReadResult{std::unexpect, source.error()}};
        ASSERT_FALSE(resolved);
        ASSERT_FALSE(resolved.error().empty());
        EXPECT_EQ(resolved.error().front().code, test.code);
        EXPECT_EQ(resolved.error().front().node_path, test.path);
    }
}
}
