#include <ioj/files.h>
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_parser.h>
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
        .teams = {TeamId::Blue},
    };
    result.entities.push_back({.id = EntityId{"player"},
                               .archetype = EntityArchetype::PlayerFighter,
                               .team = TeamId::Blue});
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

TEST(NativeLevelAuthoringReader, ReadsFileWithRootRelativeLibrary) {
    TemporaryLevelDirectory directory;
    std::ofstream{directory.path() / "Libraries" / "metadata.scm"}
        << "(define benchmark-title \"Loaded From Library\")";
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "benchmark-title");
    auto const path{directory.path() / "level.scm"};
    std::ofstream{path} << "(load-script \"Libraries/metadata.scm\")\n" << source;
    auto const result{DefinitionReader{}.read_level_file(path)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->metadata.title, "Loaded From Library");
}
TEST(NativeLevelAuthoringReader, ReportsMissingFile) {
    auto const result{DefinitionReader{}.read_level_file("missing-level.scm")};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().front().code, DiagnosticCode::FileOpenFailed);
    EXPECT_EQ(result.error().front().source_path, "missing-level.scm");
}
TEST(NativeLevelAuthoringReader, RootControlsImportsAndPreservesFailuresAndSourcePaths) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "shared-title");
    std::ofstream{root / "first.scm"} << "(define shared-title \"Shared\")\n" << source;
    std::ofstream{root / "second.scm"} << source;
    std::ofstream{root / "broken.scm"} << "(undefined-function)";
    std::ofstream{root / "ignored.scm"} << "(error 'ignored \"Must not be loaded\")";
    std::ofstream{root / "campaign.scm"}
        << "(campaign :id 'shared :title shared-title :levels '(test))";
    std::ofstream{root / "catalog.scm"} << R"(
        (catalog
          :levels (list "first.scm"
                        "broken.scm"
                        "second.scm")
          :campaigns (list "campaign.scm")))";
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), 3u);
    ASSERT_EQ(catalog->campaigns.size(), 1u);
    ASSERT_TRUE(catalog->levels[0].definition);
    EXPECT_EQ(catalog->levels[0].definition->metadata.title, "Shared");
    ASSERT_FALSE(catalog->levels[1].definition);
    EXPECT_EQ(catalog->levels[1].definition.error().front().source_path, root / "broken.scm");
    ASSERT_TRUE(catalog->levels[2].definition);
    EXPECT_EQ(catalog->levels[2].definition->metadata.title, "Shared");
    ASSERT_TRUE(catalog->campaigns[0].definition);
    EXPECT_EQ(catalog->campaigns[0].definition->title, "Shared");

    std::ofstream{root / "variation.scm"} << "(catalog :levels (list \"second.scm\"))";
    auto const variation{DefinitionReader{}.read_root_file(root / "variation.scm")};
    ASSERT_TRUE(variation);
    ASSERT_FALSE(variation->levels[0].definition);
    EXPECT_EQ(variation->levels[0].definition.error().front().code,
              DiagnosticCode::ScriptEvaluationFailed);
    EXPECT_EQ(catalog->levels[0].definition->metadata.title, "Shared");
}
TEST(NativeLevelAuthoringReader, RootLoadsSharedLibrariesOnce) {
    TemporaryLevelDirectory directory;
    auto const& root{directory.path()};
    std::ofstream{root / "Libraries" / "count.scm"}
        << "(set! library-load-count (+ library-load-count 1))";
    auto source{minimal_source()};
    source.replace(source.find("\"Test\""), 6, "(number->string library-load-count)");
    std::ofstream{root / "level.scm"} << "(load-script \"Libraries/count.scm\")\n" << source;
    std::ofstream{root / "catalog.scm"} << R"(
        (define library-load-count 0)
        (load-script "Libraries/count.scm")
        (catalog :levels (list "level.scm"
                               "level.scm")))";
    auto const catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    ASSERT_EQ(catalog->levels.size(), 2u);
    for (auto const& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        EXPECT_EQ(entry.definition->metadata.title, "1");
    }
}
TEST(NativeLevelAuthoringReader, RootReportsEvaluationAndGrammarErrors) {
    TemporaryLevelDirectory directory;
    auto const root{directory.path() / "catalog.scm"};
    std::ofstream{root} << "(undefined-root-function)";
    auto const evaluation{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(evaluation);
    EXPECT_EQ(evaluation.error().front().code, DiagnosticCode::ScriptEvaluationFailed);
    EXPECT_EQ(evaluation.error().front().source_path, root);
    std::ofstream{root} << "(catalog :levels 42 :unexpected #t)";
    auto const grammar{DefinitionReader{}.read_root_file(root)};
    ASSERT_FALSE(grammar);
    EXPECT_GE(grammar.error().size(), 2u);
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
    DefinitionReader reader;
    auto const result{reader.read_campaign_source(
        R"((campaign :id 'first-campaign :title "First Campaign" :levels '(alpha beta)))")};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->id, CampaignId{"first-campaign"});
    EXPECT_EQ(result->level_ids, (std::vector{LevelId{"alpha"}, LevelId{"beta"}}));
    auto const duplicate{reader.read_campaign_source(
        R"((campaign :id 'campaign :title "Duplicate" :levels '(alpha alpha)))")};
    ASSERT_FALSE(duplicate);
    EXPECT_TRUE(has_code(duplicate.error(), DiagnosticCode::DuplicateCampaignLevel));
}
TEST(NativeLevelAuthoringReader, ParsesClosedVocabulariesAndPreservesUnpopulatedTeams) {
    auto source{minimal_source()};
    source.replace(source.find(":teams '(blue)"), 14, ":teams '(blue red)");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_TRUE(result) << format_diagnostics(result.error());
    EXPECT_EQ(result->teams, (std::vector{TeamId::Blue, TeamId::Red}));
    ASSERT_EQ(result->entities.size(), 1u);
    EXPECT_EQ(result->entities.front().team, TeamId::Blue);
    EXPECT_EQ(result->entities.front().archetype, EntityArchetype::PlayerFighter);
}
TEST(NativeLevelAuthoringReader, RejectsUnknownTeamAndArchetypeIndependently) {
    auto source{minimal_source()};
    source.replace(source.find("'player-fighter"), 15, "'unknown-ship");
    source.replace(source.find(":team 'blue"), 11, ":team 'unknown-team");
    auto const result{DefinitionReader{}.read_level_source(source)};
    ASSERT_FALSE(result);
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedArchetype));
    EXPECT_TRUE(has_code(result.error(), DiagnosticCode::UnsupportedTeamId));
}
TEST(NativeLevelAuthoringReader, ReportsIndependentMalformedProperties) {
    auto const result{DefinitionReader{}.read_level_source(R"((level
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
        EXPECT_FALSE(DefinitionReader{}.read_level_source(source)) << source;
    }
    auto const missing{DefinitionReader{}.read_campaign_source("(campaign :id 'x)")};
    ASSERT_FALSE(missing);
    EXPECT_TRUE(has_code(missing.error(), DiagnosticCode::MissingProperty));
}
TEST(NativeLevelAuthoringReader, DistinguishesEvaluationConversionAndParsingFailures) {
    auto const evaluation{DefinitionReader{}.read_level_source("(missing-function)")};
    auto const conversion{DefinitionReader{}.read_level_source("(lambda () 1)")};
    auto const parsing{DefinitionReader{}.read_level_source("'(1 2)")};
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
    first.teams.push_back(TeamId::Red);
    first.entities.push_back({.id = EntityId{"red-capital"},
                              .archetype = EntityArchetype::CapitalShip,
                              .team = TeamId::Red,
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
    auto const loaded{DefinitionReader{}.read_level_source(*a)};
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
    auto parsed{DefinitionReader{}.read_level_source(*source)};
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
            definition.collision_grid->level_size = ml::Vector3d{20000, 20000, 20000};
        }
        if ((mask & 2) != 0) {
            definition.collision_grid->cell_size = ml::Vector3d{5000, 5000, 5000};
        }
        auto const source{emit_editor_level_source(definition)};
        ASSERT_TRUE(source) << format_diagnostics(source.error());
        auto const parsed{DefinitionReader{}.read_level_source(*source)};
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
        EXPECT_FALSE(DefinitionReader{}.read_level_source(source)) << grid;
    }
}
TEST(NativeLevelAuthoringWriter, RoundTripsInitialMissionObjectives) {
    auto definition{make_level("mission")};
    definition.teams.push_back(TeamId::Red);
    definition.entities.push_back(
        {.id = EntityId{"enemy"}, .archetype = EntityArchetype::CapitalShip, .team = TeamId::Red});
    definition.mission = LevelMissionDefinition{.mode = LevelMissionMode::KillEnemies,
                                                .kill_count = 2,
                                                .hero_entity_ids = {EntityId{"player"}},
                                                .must_survive_entity_ids = {EntityId{"player"}},
                                                .required_kill_entity_ids = {EntityId{"enemy"}}};
    auto const source{emit_editor_level_source(definition)};
    ASSERT_TRUE(source);
    auto const parsed{DefinitionReader{}.read_level_source(*source)};
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
    auto catalog{DefinitionReader{}.read_root_file(root / "catalog.scm")};
    ASSERT_TRUE(catalog) << format_diagnostics(catalog.error());
    for (auto& entry : catalog->levels) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        levels.emplace_back(levels.size(), entry.source_path, std::move(*entry.definition));
    }
    for (auto& entry : catalog->campaigns) {
        ASSERT_TRUE(entry.definition) << format_diagnostics(entry.definition.error());
        campaigns.emplace_back(campaigns.size(), entry.source_path, std::move(*entry.definition));
    }
    for (auto const& entry : std::filesystem::directory_iterator(root / "Benchmarks")) {
        if (entry.path().extension() == ".scm") {
            auto const definition{DefinitionReader{root}.read_level_file(entry.path())};
            ASSERT_TRUE(definition) << format_diagnostics(definition.error());
        }
    }
    EXPECT_GT(levels.size(), 30u);
    EXPECT_GT(campaigns.size(), 5u);
    EXPECT_TRUE(validate_level_catalog(levels).empty());
    EXPECT_TRUE(validate_campaign_catalog(campaigns, levels).empty());
}
}
