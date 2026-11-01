#include <ioj/sim/telemetry/report_json.h>
#include <ioj/sim/telemetry/run_catalog.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace ioj::sim::telemetry::tests {
class CatalogFiles : public ::testing::Test {
  protected:
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
                    ("telemetry-catalog-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    void write(std::string const& id, std::string const& level, std::string const& launch) {
        telemetry::Report report;
        report.run.metadata.run_id = id;
        report.run.metadata.map_name = level;
        report.run.metadata.level_id = level;
        report.run.metadata.level_display_name = level;
        report.run.metadata.launched_utc = launch;
        report.run.completion.completed_utc = launch;
        auto const json{telemetry::serialize_report(report)};
        ASSERT_TRUE(json) << json.error();
        std::ofstream{directory / (id + ".json")} << *json;
    }
    std::filesystem::path directory;
};

TEST_F(CatalogFiles, RefreshesFiltersPreservesSelectionAndReportsCorruption) {
    write("older", "Alpha", "2026-01-01T00:00:00Z");
    write("newer", "Beta", "2026-02-01T00:00:00Z");
    std::ofstream{directory / "broken.json"} << "not json";
    std::ifstream file{directory / "older.json"};
    std::string invalid{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    auto const reason{invalid.find("world_end")};
    ASSERT_NE(reason, std::string::npos);
    invalid.replace(reason, std::string_view{"world_end"}.size(), "future_reason");
    std::ofstream{directory / "unknown_reason.json"} << invalid;

    RunCatalog catalog{directory};
    catalog.refresh();
    EXPECT_EQ(catalog.get_unreadable_file_count(), 2);
    ASSERT_EQ(catalog.get_runs().size(), 2);
    EXPECT_EQ(catalog.get_runs()[0].run_id, "newer");
    ASSERT_NE(catalog.get_selected_record(), nullptr);
    EXPECT_EQ(catalog.get_selected_record()->run.metadata.level_id, "Beta");
    EXPECT_TRUE(catalog.select_run("older"));
    catalog.refresh();
    EXPECT_EQ(catalog.get_selected_run_id(), "older");
    catalog.set_level_filter("Beta");
    ASSERT_EQ(catalog.get_runs().size(), 1);
    EXPECT_EQ(catalog.get_selected_run_id(), "newer");
    EXPECT_FALSE(catalog.select_run("older"));
    catalog.set_level_filter("unknown");
    EXPECT_EQ(catalog.get_level_filter(), "All levels");
}

TEST_F(CatalogFiles, RefreshInvalidatesMissingOrCorruptBaselinesAndSelections) {
    write("first", "Alpha", "2026-02-01T00:00:00Z");
    write("baseline", "Alpha", "2026-01-01T00:00:00Z");
    write("other", "Beta", "2026-03-01T00:00:00Z");
    RunCatalog catalog{directory};
    catalog.refresh();
    ASSERT_TRUE(catalog.select_run("first"));
    ASSERT_TRUE(catalog.select_baseline("baseline"));
    EXPECT_FALSE(catalog.select_baseline("other"));
    EXPECT_TRUE(catalog.get_baseline_run_id().empty());
    ASSERT_TRUE(catalog.select_baseline("baseline"));
    std::ofstream{directory / "baseline.json"} << "corrupt";
    catalog.refresh();
    EXPECT_EQ(catalog.get_baseline_record(), nullptr);
    EXPECT_TRUE(catalog.get_baseline_run_id().empty());
    EXPECT_EQ(catalog.get_selected_run_id(), "first");

    std::filesystem::remove(directory / "first.json");
    EXPECT_FALSE(catalog.select_run("first"));
    EXPECT_NE(catalog.get_selected_error().find("Could not read"), std::string::npos);
    EXPECT_EQ(catalog.get_selected_record(), nullptr);
    catalog.refresh();
    EXPECT_EQ(catalog.get_selected_run_id(), "other");
    EXPECT_TRUE(catalog.get_selected_error().empty());
}
}
