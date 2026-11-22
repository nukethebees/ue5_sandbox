#include "module_fixture.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/level_inputs_writer.h>
#include <ioj/settings/settings_values_reader.h>
#include <ioj/settings/settings_values_writer.h>
#include <ioj/testing/files.h>

#include <gtest/gtest.h>

#include <chrono>
#include <format>

namespace ioj::levels::authoring::tests {
namespace {
template <typename T>
auto decode_record(std::string_view source) -> T {
    auto const ast{read_test_value(DefinitionReader{}, source).value()};
    return s7::read<T>(s7::Reader{ast}, ast.root, "record").value();
}
template <typename T>
auto apply_record(T const& base, std::string_view source) -> T {
    auto const ast{read_test_value(DefinitionReader{}, source).value()};
    return s7::read_override(s7::Reader{ast}, ast.root, "record", base).value();
}
}

TEST(SchemeRecords, FunctionalUpdatesPreserveOriginalAndNestedValues) {
    EXPECT_TRUE(decode_record<bool>(R"(
      (let* ((ship (entity :id 'enemy :health 10))
             (base (level :title "Original" :entities (list ship)))
             (changed (copy-with base :title "Changed" :description "Added")))
        (and (level? base) (not (entity? base)) (not (level? #f))
             (equal? (level-ref base :title) "Original")
             (equal? (level-ref changed :title) "Changed")
             (equal? (level-ref changed :description) "Added")
             (eq? (car (level-ref changed :entities)) ship)
             (= (entity-ref (copy-with ship :health 20) :health) 20)
             (= (entity-ref ship :health) 10))))"));
    EXPECT_TRUE(decode_record<bool>(R"(
      (let* ((base (settings :tick (tick :tick-rate 60 :time-scale 1)))
             (changed (copy-with base :tick (copy-with (settings-ref base :tick) :tick-rate 90))))
        (and (= (tick-ref (settings-ref base :tick) :tick-rate) 60)
             (= (tick-ref (settings-ref changed :tick) :tick-rate) 90)
             (= (tick-ref (settings-ref changed :tick) :time-scale) 1))))"));
    EXPECT_FALSE(decode_record<bool>(R"((radar-ref (radar :enabled #f) :enabled))"));
}

TEST(SchemeRecords, RejectMalformedAccessAndUpdates) {
    for (auto const source : {"(level-ref (level) :title)",
                              "(copy-with 1 :title 1)",
                              "(level-ref (entity :title 1) :title)",
                              "(copy-with (level) :unknown 1)",
                              "(copy-with (level) :title 1 :title 2)",
                              "(copy-with (level) :title)",
                              "(copy-with '(:level :title) :title 1)",
                              "(copy-with '(:level :title 1 :title 2) :description 3)"}) {
        EXPECT_FALSE(read_test_value(DefinitionReader{}, source)) << source;
    }
}

TEST(SchemeRecords, PositionalRecordsUseSchemaFieldNames) {
    auto const input{decode_record<UnlockInput>(R"(
      (let* ((base (level-completed 'first))
             (changed (copy-with base :level-id 'second)))
        (if (and (level-completed? changed)
                 (eq? (level-completed-ref base :level-id) 'first))
            changed
            #f)))")};
    EXPECT_EQ(input.level, LevelId{"second"});
    EXPECT_EQ(decode_record<UnlockInput>(s7::serialize(input)).level, input.level);
}

TEST(SchemeSerialization, SettingsRoundTripWithNestedDifferencesAndFalseValues) {
    settings::Settings base;
    auto changed{base};
    changed.fighters.speed = 8123.456f;
    changed.fighters.attack_distance_band = settings::AttackDistanceBand::make({0.4f, 0.55f, 0.6f});
    changed.presentation.radar.enabled = false;
    changed.collision_grid.cell_size = {123.4567f, 9876.543f, 22222.25f};
    auto const complete{s7::serialize(changed)};
    EXPECT_EQ(decode_record<settings::Settings>(complete), changed);
    auto const differences{s7::serialize(changed, base)};
    EXPECT_EQ(apply_record(base, differences), changed);
    EXPECT_EQ(s7::serialize(base, base), "(settings)");
    auto resource_changes{base};
    resource_changes.capital_ships.fighter_spawn_slots_relative_transforms.resize(2);
    resource_changes.turrets.fire_point_offset = {1.f, 2.f, 3.f};
    resource_changes.tube_spinners.fire_point_offsets.push_back({{4.f, 5.f, 6.f}, {}});
    EXPECT_EQ(s7::serialize(resource_changes, base), "(settings)");
    EXPECT_TRUE(s7::equivalent(std::optional{resource_changes}, std::optional{base}));
    EXPECT_TRUE(s7::equivalent(std::vector{resource_changes}, std::vector{base}));
}

TEST(SchemeSerialization, SymbolsRemainSymbolsAndOptionalRecordsPreserveTheirBase) {
    auto base{decode_record<EntityInput>(R"(
      (entity :id 'enemy :archetype 'static-turret :team 'red :position '(0 0 0) :health 50))")};
    for (auto const name : {"with spaces", "123", "semi;colon", "quote\"slash\\"}) {
        auto changed{base};
        changed.id = EntityId{name};
        changed.health.reset();
        EXPECT_TRUE(s7::equivalent(decode_record<EntityInput>(s7::serialize(changed)), changed));
        EXPECT_TRUE(s7::equivalent(apply_record(base, s7::serialize(changed, base)), changed));
    }
}

TEST(SchemeRecords, HumanRootComposesAnIncompleteGeneratedBaseline) {
    auto const directory{std::filesystem::temp_directory_path() /
                         std::format("composed-level-{}",
                                     std::chrono::steady_clock::now().time_since_epoch().count())};
    std::filesystem::create_directories(directory / "Example");
    std::filesystem::create_directories(directory / "Settings");
    auto const entry{directory / "Example/Example.scm"};
    auto const config{directory / "Example/editor-config.scm"};
    auto const base{directory / "Settings/level-defaults.scm"};
    ASSERT_TRUE(testing::write_files(
        {{base,
          "(export game-settings) (define game-settings (settings :fighters (fighters :speed 4000) "
          ":tick (tick :tick-rate 60 :time-scale 1)))"},
         {config,
          R"((export scene) (define scene (level :id 'example :title "Example" :player 'pilot
                   :entities (list (entity :id 'carrier :archetype 'capital-ship :team 'blue
                                           :position '(0 0 0) :initial-spawn-delay 0))
                   :settings (settings :fighters (fighters :speed 4000) :tick (tick :time-scale 2)))))"},
         {entry,
          R"((export mission) (define base (module-ref (import-module "Example/editor-config.scm") 'scene))
          (define mission (copy-with base :settings (merge-settings (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
                                       (level-ref base :settings) (settings :tick (tick :time-scale 3)))
            :entities (append (level-ref base :entities)
            (list (entity :id 'pilot :archetype 'player-fighter :team 'blue :position '(0 0 0)))))))"}}));
    auto const scene{DefinitionReader{}.read_script_file(config, "scene")};
    ASSERT_TRUE(scene) << format_diagnostics(scene.error());
    auto const input{s7::read<LevelInput>(s7::Reader{*scene}, scene->root, "level")};
    ASSERT_TRUE(input);
    auto const exported{emit_editor_level_source(*input, &*scene)};
    EXPECT_FALSE(exported.contains(":tick-rate"));
    EXPECT_FALSE(exported.contains(":none"));
    EXPECT_FALSE(exported.contains(":rotation"));
    EXPECT_FALSE(exported.contains(":spawn-at"));
    ASSERT_TRUE(testing::write_files({{config, exported},
                                      {base,
                                       "(export game-settings) (define game-settings (settings "
                                       ":fighters (fighters :speed 5000) :tick (tick "
                                       ":tick-rate 90 :time-scale 1)))"}}));
    auto const level{DefinitionReader::for_level(entry).read_level_file(entry)};
    ASSERT_TRUE(level) << format_diagnostics(level.error());
    EXPECT_EQ(level->settings().fighters.speed, 4000.f);
    EXPECT_EQ(level->settings().tick.tick_rate.hz(), 90.);
    EXPECT_EQ(level->settings().tick.time_scale.get(), 3.);
    ASSERT_EQ(level->entities().size(), 2u);
    EXPECT_TRUE(
        s7::equivalent(make_entity_input(level->entities().front()), input->entities.front()));
    EXPECT_EQ(level->player_entity_id(), EntityId{"pilot"});
    std::filesystem::remove_all(directory);
}
}
