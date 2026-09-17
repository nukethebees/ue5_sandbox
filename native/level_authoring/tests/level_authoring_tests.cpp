#include <sandbox/level_authoring/CampaignDefinitionReader.h>
#include <sandbox/level_authoring/CatalogValidation.h>
#include <sandbox/level_authoring/LevelDefinitionReader.h>
#include <sandbox/level_authoring/LevelDefinitionWriter.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace ml::level_authoring {
namespace {
auto make_level(std::string id, std::vector<std::string> unlocks = {})
    -> ::ioj::sim::levels::LevelDefinition {
    ::ioj::sim::levels::LevelDefinition definition{
        .metadata = {.id = std::move(id), .title = "Test Level"},
        .unlock_level_ids = std::move(unlocks),
        .player_entity_id = "player",
        .teams = {"blue"},
    };
    definition.entities.push_back({
        .id = "player",
        .archetype = "player-fighter",
        .team = "blue",
    });
    return definition;
}

auto contains_message(std::vector<CatalogValidationIssue> const& issues,
                      std::string_view const text) -> bool {
    return std::ranges::any_of(issues,
                               [text](auto const& issue) { return issue.message.contains(text); });
}

class TemporaryLevelDirectory final {
  public:
    TemporaryLevelDirectory() {
        auto const suffix{std::chrono::steady_clock::now().time_since_epoch().count()};
        path_ = std::filesystem::temp_directory_path() /
                ("sandbox-level-reader-" + std::to_string(suffix));
        std::filesystem::create_directories(path_ / "Libraries");
    }
    ~TemporaryLevelDirectory() { std::filesystem::remove_all(path_); }

    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_{};
};

TEST(NativeLevelAuthoringReader, ReadsFileWithSiblingLibraryDirectory) {
    TemporaryLevelDirectory directory;
    std::ofstream{directory.path() / "Libraries" / "metadata.scm"}
        << "(define benchmark-title \"Loaded From Library\")";
    auto const level_path{directory.path() / "level.scm"};
    std::ofstream{level_path} << R"(
(load-script "metadata.scm")
(level
  (id 'file-level)
  (title benchmark-title)
  (teams (team 'blue))
  (player 'player)
  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0)
      (rotation 0 0 0))))
)";

    LevelDefinitionReader reader;
    auto const result{reader.read_file(level_path)};

    ASSERT_TRUE(result) << result.script_error;
    EXPECT_EQ(result.definition->metadata.title, "Loaded From Library");
}

TEST(NativeLevelAuthoringReader, ReportsMissingFile) {
    LevelDefinitionReader reader;
    auto const result{reader.read_file("missing-level.scm")};

    ASSERT_FALSE(result);
    EXPECT_TRUE(result.script_error.contains("Unable to open level file"));
}

TEST(NativeLevelAuthoringCampaignReader, DecodesAndValidatesCampaignData) {
    CampaignDefinitionReader reader;
    auto const result{reader.read_source(R"(
(campaign
  (id 'first-campaign)
  (title "First Campaign")
  (levels 'alpha 'beta))
)")};

    ASSERT_TRUE(result);
    EXPECT_EQ(result.definition->id, "first-campaign");
    EXPECT_EQ(result.definition->title, "First Campaign");
    EXPECT_EQ(result.definition->level_ids, (std::vector<std::string>{"alpha", "beta"}));

    auto const duplicate{reader.read_source(
        "(campaign (id 'duplicate) (title \"Duplicate\") (levels 'alpha 'alpha))")};
    ASSERT_FALSE(duplicate);
    ASSERT_FALSE(duplicate.decode_errors.empty());
    EXPECT_TRUE(duplicate.decode_errors.front().message.contains("duplicated"));
}

TEST(NativeLevelAuthoringWriter, EmitsDeterministicReadableSource) {
    auto first{make_level("writer-example")};
    first.metadata.title = "Writer \"Example\"";
    first.metadata.description = "Line one\nLine two.";
    first.teams.push_back("red");
    first.entities.push_back({
        .id = "red-capital",
        .archetype = "capital-ship",
        .team = "red",
        .position = {1000.0, 200.12349, -300.5},
        .rotation = {0.0, 90.0, 0.0},
    });
    auto second{first};
    std::ranges::reverse(second.teams);
    std::ranges::reverse(second.entities);

    auto const first_source{emit_editor_level_source(first)};
    auto const second_source{emit_editor_level_source(second)};
    ASSERT_TRUE(first_source);
    ASSERT_TRUE(second_source);
    EXPECT_EQ(*first_source, *second_source);
    EXPECT_TRUE(first_source->contains("(title \"Writer \\\"Example\\\"\")"));
    EXPECT_TRUE(first_source->contains("(position 1000 200.123 -300.5)"));

    LevelDefinitionReader reader;
    auto const decoded{reader.read_source(*first_source)};
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded.definition->entities.size(), 2u);
}

TEST(NativeLevelAuthoringWriter, RejectsRuntimeOnlyFields) {
    auto definition{make_level("scheduled")};
    definition.entities.front().spawn_time_seconds = 1.0;
    auto const result{emit_editor_level_source(definition)};

    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().contains("initial t=0"));
}

TEST(NativeLevelAuthoringWriter, RoundTripsInitialMissionObjectives) {
    auto definition{make_level("mission-writer")};
    definition.teams.push_back("red");
    definition.entities.push_back({
        .id = "enemy",
        .archetype = "capital-ship",
        .team = "red",
    });
    definition.mission = ::ioj::sim::levels::LevelMissionDefinition{
        .mode = ::ioj::sim::levels::LevelMissionMode::KillEnemies,
        .kill_count = 2,
        .hero_entity_ids = {"player"},
        .must_survive_entity_ids = {"player"},
        .required_kill_entity_ids = {"enemy"},
    };

    auto const source{emit_editor_level_source(definition)};
    ASSERT_TRUE(source);
    EXPECT_TRUE(source->contains("(mission"));
    EXPECT_TRUE(source->contains("(mode 'kill-enemies)"));
    EXPECT_TRUE(source->contains("(required-kills 'enemy)"));

    LevelDefinitionReader reader;
    auto const decoded{reader.read_source(*source)};
    ASSERT_TRUE(decoded) << decoded.script_error;
    ASSERT_TRUE(decoded.definition->mission);
    EXPECT_EQ(decoded.definition->mission->mode, ::ioj::sim::levels::LevelMissionMode::KillEnemies);
    EXPECT_EQ(decoded.definition->mission->kill_count, 2);
    EXPECT_EQ(decoded.definition->mission->required_kill_entity_ids,
              (std::vector<std::string>{"enemy"}));
}

TEST(NativeLevelAuthoringCatalog, RejectsDuplicatesCyclesAndUnavailableDependencies) {
    std::vector<LevelCatalogEntry> duplicates{
        {.filename = "first.scm", .definition = make_level("same")},
        {.filename = "second.scm", .definition = make_level("same")},
    };
    auto const duplicate_issues{validate_level_catalog(duplicates)};
    EXPECT_EQ(duplicate_issues.size(), 2u);
    EXPECT_TRUE(contains_message(duplicate_issues, "declared by both"));

    std::vector<LevelCatalogEntry> cycles{
        {.filename = "alpha.scm", .definition = make_level("alpha", {"beta"})},
        {.filename = "beta.scm", .definition = make_level("beta", {"alpha"})},
    };
    auto const cycle_issues{validate_level_catalog(cycles)};
    EXPECT_EQ(cycle_issues.size(), 2u);
    EXPECT_TRUE(contains_message(cycle_issues, "Unlock dependency cycle"));

    std::vector<LevelCatalogEntry> unavailable{
        {.filename = "alpha.scm", .definition = make_level("alpha", {"missing"})},
        {.filename = "beta.scm", .definition = make_level("beta", {"alpha"})},
    };
    auto const unavailable_issues{validate_level_catalog(unavailable)};
    EXPECT_EQ(unavailable_issues.size(), 2u);
    EXPECT_TRUE(contains_message(unavailable_issues, "requires unavailable"));
}

TEST(NativeLevelAuthoringCatalog, ValidatesCampaignIdsAndLevelReferences) {
    std::vector<LevelCatalogEntry> levels{
        {.filename = "alpha.scm", .definition = make_level("alpha")},
    };
    std::vector<CampaignCatalogEntry> campaigns{
        {.filename = "first.scm",
         .definition = CampaignDefinition{.id = "campaign", .level_ids = {"alpha"}}},
        {.filename = "second.scm",
         .definition = CampaignDefinition{.id = "campaign", .level_ids = {"missing"}}},
    };
    auto const duplicate_issues{validate_campaign_catalog(campaigns, levels, {})};
    EXPECT_EQ(duplicate_issues.size(), 2u);
    EXPECT_TRUE(contains_message(duplicate_issues, "declared by both"));

    campaigns.resize(1);
    campaigns.front().definition->id = "valid-id";
    campaigns.front().definition->level_ids = {"missing"};
    auto const missing_issues{validate_campaign_catalog(campaigns, levels, {})};
    ASSERT_EQ(missing_issues.size(), 1u);
    EXPECT_TRUE(missing_issues.front().message.contains("references unavailable"));
}
} // namespace
} // namespace ml::level_authoring
