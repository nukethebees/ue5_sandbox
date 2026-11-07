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
    auto const ast{DefinitionReader{}.read_source_ast(source).value()};
    return s7::read<T>(s7::Reader{ast}, ast.root, "record").value();
}
template <typename T>
auto apply_record(T const& base, std::string_view source) -> T {
    auto const ast{DefinitionReader{}.read_source_ast(source).value()};
    return s7::read_override(s7::Reader{ast}, ast.root, "record", base).value();
}
}

TEST(SchemeRecords, FunctionalUpdatesPreserveOriginalAndNestedValues) {
    EXPECT_TRUE(decode_record<bool>(R"(
      (let* ((ship (entity :id 'enemy :health 10))
             (base (level :title "Original" :entities (list ship)))
             (changed (level-with base :title "Changed" :description "Added")))
        (and (level? base) (not (entity? base)) (not (level? #f))
             (equal? (level-ref base :title) "Original")
             (equal? (level-ref changed :title) "Changed")
             (equal? (level-ref changed :description) "Added")
             (eq? (car (level-ref changed :entities)) ship)
             (= (entity-ref (entity-with ship :health 20) :health) 20)
             (= (entity-ref ship :health) 10))))"));
    EXPECT_FALSE(decode_record<bool>(R"((radar-ref (radar :enabled #f) :enabled))"));
}

TEST(SchemeRecords, RejectMalformedAccessAndUpdates) {
    for (auto const source : {"(level-ref (level) :title)",
                              "(level-ref (entity :title 1) :title)",
                              "(level-with (level) :unknown 1)",
                              "(level-with (level) :title 1 :title 2)",
                              "(level-with (level) :title)",
                              "(level-with '(:level :title) :title 1)",
                              "(level-with '(:level :title 1 :title 2) :description 3)"}) {
        EXPECT_FALSE(DefinitionReader{}.read_source_ast(source)) << source;
    }
}

TEST(SchemeRecords, PositionalRecordsUseSchemaFieldNames) {
    auto const input{decode_record<UnlockInput>(R"(
      (let* ((base (level-completed 'first))
             (changed (level-completed-with base :level-id 'second)))
        (if (and (level-completed? changed)
                 (eq? (level-completed-ref base :level-id) 'first))
            changed
            #f)))")};
    EXPECT_EQ(input.level, LevelId{"second"});
    EXPECT_EQ(decode_record<UnlockInput>(s7::serialize(input)), input);
}

TEST(SchemeSerialization, CompleteLevelInputsPreserveAllAuthoredFields) {
    auto original{decode_record<LevelInput>(R"(
      (level :id 'roundtrip :title "Quote \" and slash \\" :description "line\nline"
        :par-time 15.25 :player 'pilot
        :entities (list
          (entity :id 'pilot :archetype 'player-fighter :team 'blue :position '(0 0 0) :health 123)
          (entity :id 'capital :archetype 'capital-ship :team 'red
            :position '(1000.1234567890123 -0.00001 20.5) :rotation '(10 20 30)
            :health 456 :initial-spawn-delay 3.25 :spawn-cooldown 7.5)
          (entity :id 'turret :archetype 'static-turret :team 'red :position '(9000 0 0)
            :health 789 :laser-damage 321 :spawn-at 30)
          (entity :id 'spinner :archetype 'tube-spinner :team 'white :position '(5000 0 0)
            :rotation '(0 45 0) :initial-fire-point 2))
        :mission (mission :objectives (list
          (objective :id 'destroy :team 'blue :wins-level 'yes
            :pass (entity-destroyed 'turret))))))")};
    auto const encoded{emit_level_source(original)};
    EXPECT_EQ(encoded, emit_level_source(original));
    EXPECT_EQ(decode_record<LevelInput>(encoded), original);
    auto const definition{DefinitionReader{}.read_level_source(encoded)};
    ASSERT_TRUE(definition) << format_diagnostics(definition.error());
    auto const resolved{make_level_input(*definition)};
    EXPECT_EQ(decode_record<LevelInput>(emit_level_source(resolved)), resolved);
}

TEST(SchemeSerialization, SparseOverridesClearOptionalsAndReplaceCollections) {
    LevelInput base;
    base.id = LevelId{"example"};
    base.title = "Example";
    base.entities.push_back({.id = EntityId{"old"}});
    base.camera = LevelCameraDefinition{{EntityId{"old"}}, {-1., 0., 0.}, 1000.};
    auto changed{base};
    changed.entities.clear();
    changed.camera.reset();
    changed.settings.fighters.speed = 4321.125f;
    auto const encoded{s7::serialize(changed, base)};
    EXPECT_TRUE(encoded.contains(":camera :none"));
    EXPECT_TRUE(encoded.contains(":entities (list)"));
    EXPECT_FALSE(encoded.contains(":title"));
    EXPECT_EQ(apply_record(base, encoded), changed);
    EXPECT_EQ(apply_record(base, s7::serialize(base, base)), base);
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
}

TEST(SchemeSerialization, SymbolsRemainSymbolsAndOptionalRecordsPreserveTheirBase) {
    auto base{decode_record<EntityInput>(R"(
      (entity :id 'enemy :archetype 'static-turret :team 'red :position '(0 0 0) :health 50))")};
    for (auto const name : {"with spaces", "123", "semi;colon", "quote\"slash\\"}) {
        auto changed{base};
        changed.id = EntityId{name};
        changed.health.reset();
        EXPECT_EQ(decode_record<EntityInput>(s7::serialize(changed)), changed);
        EXPECT_EQ(apply_record(base, s7::serialize(changed, base)), changed);
    }
    LevelInput level;
    level.id = LevelId{"example"};
    level.title = "Example";
    level.camera = LevelCameraDefinition{{EntityId{"enemy"}}, {-1., 0., 0.}, 1000.};
    auto changed{level};
    changed.camera->distance = 2000.;
    EXPECT_EQ(apply_record(level, s7::serialize(changed, level)), changed);
}

TEST(SchemeRecords, HumanRootComposesAnIncompleteGeneratedBaseline) {
    auto const directory{std::filesystem::temp_directory_path() /
                         std::format("composed-level-{}",
                                     std::chrono::steady_clock::now().time_since_epoch().count())};
    std::filesystem::create_directories(directory / "Example");
    ASSERT_TRUE(testing::write_files(
        {{directory / "Example/editor-config.scm",
          R"((level :id 'example :title "Example" :player 'pilot :entities (list)))"},
         {directory / "Example.scm", R"((let ((base (load-script "Example/editor-config.scm")))
          (level-with base :entities (append (level-ref base :entities)
            (list (entity :id 'pilot :archetype 'player-fighter :team 'blue :position '(0 0 0)))))))"}}));
    auto const level{DefinitionReader{}.read_level_file(directory / "Example.scm")};
    ASSERT_TRUE(level) << format_diagnostics(level.error());
    EXPECT_EQ(level->entities().size(), 1u);
    EXPECT_EQ(level->player_entity_id(), EntityId{"pilot"});
    std::filesystem::remove_all(directory);
}
}
