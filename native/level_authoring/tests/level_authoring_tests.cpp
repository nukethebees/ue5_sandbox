#include <ioj/files.h>
#include <ioj/levels/authoring/campaign_definition_reader.h>
#include <ioj/levels/authoring/level_definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/catalog_validation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace ioj::levels::authoring::tests {
namespace {
auto make_level(std::string id, std::vector<LevelId> unlocks = {}) -> LevelDefinition {
    LevelDefinition result{
        .metadata = {.id = LevelId{std::move(id)}, .title = "Test Level"},
        .unlock_level_ids = std::move(unlocks),
        .player_entity_id = EntityId{"player"},
        .teams = {TeamId{"blue"}},
    };
    result.entities.push_back(
        {.id = EntityId{"player"}, .archetype = "player-fighter", .team = TeamId{"blue"}});
    return result;
}
auto has_code(Diagnostics const& errors, DiagnosticCode const code) -> bool {
    return std::ranges::any_of(errors,
                               [code](Diagnostic const& error) { return error.code == code; });
}
auto minimal_source() -> std::string {
    return R"((level :id 'test :title "Test" :teams '(blue) :player 'player
      :entities (list (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 0 0) :rotation '(0 0 0)))))";
}
class TemporaryLevelDirectory {
  public:
    TemporaryLevelDirectory() {
        path_ =
            std::filesystem::temp_directory_path() /
            std::filesystem::path{std::format(
                L"level-\u03bb-{}", std::chrono::steady_clock::now().time_since_epoch().count())};
        std::filesystem::create_directories(path_ / "Libraries");
    }
    ~TemporaryLevelDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_{};
};
}

static_assert(!std::is_convertible_v<LevelId, CampaignId>);
static_assert(!std::is_convertible_v<EntityId, TeamId>);
static_assert(!std::is_convertible_v<std::string, LevelId>);

TEST(NativeLevelAuthoringReader, ReadsFileWithSiblingLibraryDirectory) {
    TemporaryLevelDirectory directory;
    std::ofstream{directory.path() / "Libraries" / "metadata.scm"}
        << "(define benchmark-title \"Loaded From Library\")";
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "benchmark-title");
    auto const path{directory.path() / "level.scm"};
    std::ofstream{path} << "(load-script \"metadata.scm\")\n" << source;
    auto const result{LevelDefinitionReader{}.read_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->metadata.title, "Loaded From Library");
}
TEST(NativeLevelAuthoringReader, ReportsMissingFile) {
    auto const result{LevelDefinitionReader{}.read_file("missing-level.scm")};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().front().code, DiagnosticCode::FileOpenFailed);
    EXPECT_EQ(result.error().front().source_path, "missing-level.scm");
}
TEST(NativeLevelAuthoringFiles, ReadsEmptyBinaryAndLargeFilesWithExplicitErrors) {
    TemporaryLevelDirectory directory;
    auto const path{directory.path() / "binary.dat"};
    {
        std::ofstream empty_file{path, std::ios::binary};
        ASSERT_TRUE(empty_file);
    }
    EXPECT_EQ(ioj::read_file(path), "");
    std::string bytes(20000, 'x');
    bytes[100] = '\0';
    bytes[8191] = '\r';
    bytes[8192] = '\n';
    {
        std::ofstream output{path, std::ios::binary};
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    EXPECT_EQ(ioj::read_file(path), bytes);
    auto const missing{ioj::read_file(directory.path() / "missing.dat")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ioj::FileReadErrorCode::OpenFailed);
    EXPECT_TRUE(missing.error().system_error);
}
TEST(NativeLevelAuthoringCampaignReader, DecodesAndValidatesCampaignData) {
    CampaignDefinitionReader reader;
    auto const result{reader.read_source(
        R"((campaign :id 'first-campaign :title "First Campaign" :levels '(alpha beta)))")};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->id, CampaignId{"first-campaign"});
    EXPECT_EQ(result->level_ids, (std::vector{LevelId{"alpha"}, LevelId{"beta"}}));
    auto const duplicate{reader.read_source(
        R"((campaign :id 'campaign :title "Duplicate" :levels '(alpha alpha)))")};
    ASSERT_FALSE(duplicate);
    EXPECT_TRUE(has_code(duplicate.error(), DiagnosticCode::DuplicateCampaignLevel));
}
TEST(NativeLevelAuthoringReader, ReportsIndependentMalformedProperties) {
    auto const result{LevelDefinitionReader{}.read_source(R"((level
      :id 17 :title 'not-text :title "duplicate" :unknown 4
      :entities (list (entity :id 4 :team 6 :archetype 7
        :position '(a b c) :rotation '(x y z) :spawn-at 'bad))))")};
    ASSERT_FALSE(result);
    EXPECT_GE(result.error().size(), 12u);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::DuplicateProperty));
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnknownProperty));
    EXPECT_TRUE(std::ranges::any_of(result.error(), [](Diagnostic const& error) {
        return error.node_path == "level.entities[0].spawn-at";
    }));
}
TEST(NativeLevelAuthoringReader, RejectsMissingPropertiesOddListsAndWrongTags) {
    for (auto const source : {"(level)",
                              "(level :id)",
                              "'(level :id x :title \"X\")",
                              "(level 'id 'x :title \"X\")",
                              "(campaign :id 'x :title \"X\")"}) {
        EXPECT_FALSE(LevelDefinitionReader{}.read_source(source)) << source;
    }
    auto const missing{CampaignDefinitionReader{}.read_source("(campaign :id 'x)")};
    ASSERT_FALSE(missing);
    EXPECT_TRUE(has_code(missing.error(), DiagnosticCode::MissingProperty));
}
TEST(NativeLevelAuthoringReader, DistinguishesEvaluationConversionAndParsingFailures) {
    auto const evaluation{LevelDefinitionReader{}.read_source("(missing-function)")};
    auto const conversion{LevelDefinitionReader{}.read_source("(lambda () 1)")};
    auto const parsing{LevelDefinitionReader{}.read_source("'(1 2)")};
    ASSERT_FALSE(evaluation);
    ASSERT_FALSE(conversion);
    ASSERT_FALSE(parsing);
    EXPECT_TRUE(has_code(evaluation.error(), DiagnosticCode::ScriptEvaluationFailed));
    EXPECT_TRUE(has_code(conversion.error(), DiagnosticCode::UnsupportedValue));
    EXPECT_TRUE(has_code(parsing.error(), DiagnosticCode::ExpectedRecord));
}
TEST(NativeLevelAuthoringWriter, EmitsDeterministicReadableSource) {
    auto first{make_level("writer-example")};
    first.metadata.title = "Writer \"Example\"";
    first.metadata.description = "Line one\nLine two.";
    first.teams.emplace_back("red");
    first.entities.push_back({.id = EntityId{"red-capital"},
                              .archetype = "capital-ship",
                              .team = TeamId{"red"},
                              .position = {1000.0, 200.12349, -300.5},
                              .rotation = {0.0, 90.0, 0.0},
                              .spawn_time_seconds = 2.5});
    auto second{first};
    std::ranges::reverse(second.teams);
    std::ranges::reverse(second.entities);
    auto const a{emit_editor_level_source(first)};
    auto const b{emit_editor_level_source(second)};
    ASSERT_TRUE(a) << format_diagnostics(a.error());
    ASSERT_TRUE(b);
    EXPECT_EQ(*a, *b);
    EXPECT_TRUE(a->contains(":position '(1000 200.123 -300.5)"));
    EXPECT_TRUE(a->contains(":spawn-at 2.5"));
    auto const loaded{LevelDefinitionReader{}.read_source(*a)};
    ASSERT_TRUE(loaded) << format_diagnostics(loaded.error());
    EXPECT_EQ(loaded->metadata.title, first.metadata.title);
    EXPECT_EQ(loaded->metadata.description, first.metadata.description);
    EXPECT_EQ(loaded->entities.size(), 2u);
}
TEST(NativeLevelAuthoringWriter, HandlesEmptyCollectionsEscapesAndNumericBoundaries) {
    auto definition{make_level("numbers")};
    definition.metadata.description = std::string{"a\0b\t\r\\\"", 7};
    definition.entities[0].position = {-0.0001, 0.0005, 1.0e100};
    auto source{emit_editor_level_source(definition)};
    ASSERT_TRUE(source) << format_diagnostics(source.error());
    EXPECT_TRUE(source->contains(":position '(0 0.001 "));
    auto parsed{LevelDefinitionReader{}.read_source(*source)};
    ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
    EXPECT_EQ(parsed->metadata.description, definition.metadata.description);
    EXPECT_DOUBLE_EQ(parsed->entities[0].position.x, 0.0);
}
TEST(NativeLevelAuthoringCollisionGrid, PreservesIndependentOverridesAcrossSourceRoundTrip) {
    for (int mask{}; mask < 4; ++mask) {
        auto definition{make_level("grid")};
        if (mask != 0) {
            definition.collision_grid.emplace();
        }
        if ((mask & 1) != 0) {
            definition.collision_grid->level_size = Vector3d{20000, 20000, 20000};
        }
        if ((mask & 2) != 0) {
            definition.collision_grid->cell_size = Vector3d{5000, 5000, 5000};
        }
        auto const source{emit_editor_level_source(definition)};
        ASSERT_TRUE(source) << format_diagnostics(source.error());
        auto const parsed{LevelDefinitionReader{}.read_source(*source)};
        ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
        ASSERT_EQ(parsed->collision_grid.has_value(), mask != 0);
        if (mask != 0) {
            EXPECT_EQ(parsed->collision_grid->level_size.has_value(), (mask & 1) != 0);
            EXPECT_EQ(parsed->collision_grid->cell_size.has_value(), (mask & 2) != 0);
        }
    }
}
TEST(NativeLevelAuthoringCollisionGrid, RejectsMalformedAndInvalidExplicitDimensions) {
    for (auto const grid :
         {"(collision-grid)",
          "(collision-grid :cell-size '(0 1 1))",
          "(collision-grid :level-size '(+nan.0 1 1))",
          "(collision-grid :level-size '(2000 2000 2000) :cell-size '(0.00001 0.00001 0.00001))",
          "(collision-grid :cell-size '(1 1 1) :cell-size '(2 2 2))",
          "(collision-grid :unknown '(1 1 1))"}) {
        auto source{minimal_source()};
        source.insert(source.size() - 1, std::format(" :collision-grid {}", grid));
        EXPECT_FALSE(LevelDefinitionReader{}.read_source(source)) << grid;
    }
}
TEST(NativeLevelAuthoringWriter, RoundTripsInitialMissionObjectives) {
    auto definition{make_level("mission")};
    definition.teams.emplace_back("red");
    definition.entities.push_back(
        {.id = EntityId{"enemy"}, .archetype = "capital-ship", .team = TeamId{"red"}});
    definition.mission = LevelMissionDefinition{.mode = LevelMissionMode::KillEnemies,
                                                .kill_count = 2,
                                                .hero_entity_ids = {EntityId{"player"}},
                                                .must_survive_entity_ids = {EntityId{"player"}},
                                                .required_kill_entity_ids = {EntityId{"enemy"}}};
    auto const source{emit_editor_level_source(definition)};
    ASSERT_TRUE(source);
    auto const parsed{LevelDefinitionReader{}.read_source(*source)};
    ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
    ASSERT_TRUE(parsed->mission);
    EXPECT_EQ(parsed->mission->mode, LevelMissionMode::KillEnemies);
    EXPECT_EQ(parsed->mission->required_kill_entity_ids,
              definition.mission->required_kill_entity_ids);
}
TEST(NativeLevelAuthoringWriter, PreservesUnsupportedEditorFeatureDiagnostics) {
    auto definition{make_level("unsupported")};
    definition.metadata.par_time_seconds = 10;
    definition.unlock_level_ids.emplace_back("other");
    definition.mission_events.push_back({});
    auto const result{emit_editor_level_source(definition)};
    ASSERT_FALSE(result);
    EXPECT_EQ(std::ranges::count(
                  result.error(), DiagnosticCode::UnsupportedEditorFeature, &Diagnostic::code),
              3);
}
TEST(NativeLevelAuthoringCatalog, RejectsDuplicatesCyclesAndUnavailableDependencies) {
    std::vector<LevelCatalogEntry> duplicates{{7, "a.scm", make_level("same")},
                                              {31, "b.scm", make_level("same")}};
    auto const duplicate_issues{validate_level_catalog(duplicates)};
    ASSERT_EQ(duplicate_issues.size(), 2u);
    EXPECT_EQ(duplicate_issues[0].entry_index, 7u);
    EXPECT_EQ(duplicate_issues[1].entry_index, 31u);
    std::vector<LevelCatalogEntry> cycles{{2, "a.scm", make_level("a", {LevelId{"b"}})},
                                          {4, "b.scm", make_level("b", {LevelId{"a"}})},
                                          {9, "c.scm", make_level("c", {LevelId{"a"}})}};
    auto const cycle_issues{validate_level_catalog(cycles)};
    EXPECT_EQ(cycle_issues.size(), 3u);
    std::vector<LevelCatalogEntry> missing{{2, "a.scm", make_level("a", {LevelId{"missing"}})},
                                           {8, "b.scm", make_level("b", {LevelId{"a"}})}};
    EXPECT_EQ(validate_level_catalog(missing).size(), 2u);
}
TEST(NativeLevelAuthoringCatalog, ValidatesCampaignIdsAndLevelReferences) {
    std::vector<LevelCatalogEntry> levels{{3, "a.scm", make_level("a")}};
    std::vector<CampaignCatalogEntry> campaigns{
        {5, "first.scm", {CampaignId{"same"}, "First", {LevelId{"a"}}}},
        {8, "second.scm", {CampaignId{"same"}, "Second", {LevelId{"missing"}}}}};
    auto const issues{validate_campaign_catalog(campaigns, levels)};
    EXPECT_EQ(issues.size(), 3u);
    campaigns.resize(1);
    campaigns[0].definition.level_ids = {LevelId{"missing"}};
    auto const missing{validate_campaign_catalog(campaigns, levels)};
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0].entry_index, 5u);
    EXPECT_EQ(missing[0].diagnostic.code, DiagnosticCode::UnavailableLevel);
}
TEST(NativeLevelAuthoringFixtures, ReadsEveryCheckedInLevelAndCampaign) {
    std::filesystem::path const root{IOJ_LEVEL_SCRIPTS};
    std::vector<LevelCatalogEntry> levels;
    std::vector<CampaignCatalogEntry> campaigns;
    LevelDefinitionReader reader{root / "Libraries"};
    for (auto const& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().extension() != ".scm" ||
            entry.path().parent_path().filename() == "Libraries" ||
            entry.path().parent_path().filename() == "Campaigns") {
            continue;
        }
        auto parsed{reader.read_file(entry.path())};
        ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
        levels.push_back({levels.size(), entry.path(), std::move(*parsed)});
    }
    CampaignDefinitionReader campaign_reader{root / "Libraries"};
    for (auto const& entry : std::filesystem::directory_iterator(root / "Campaigns")) {
        if (entry.path().extension() != ".scm") {
            continue;
        }
        auto parsed{campaign_reader.read_file(entry.path())};
        ASSERT_TRUE(parsed) << format_diagnostics(parsed.error());
        campaigns.push_back({campaigns.size(), entry.path(), std::move(*parsed)});
    }
    EXPECT_GT(levels.size(), 30u);
    EXPECT_GT(campaigns.size(), 5u);
    EXPECT_TRUE(validate_level_catalog(levels).empty());
    EXPECT_TRUE(validate_campaign_catalog(campaigns, levels).empty());
}
}
