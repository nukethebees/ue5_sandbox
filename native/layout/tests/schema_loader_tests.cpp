#include "document_test_support.hpp"
#include <ioj/layout/document.hpp>

#include <lispb/diagnostics.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace ioj::layout {
namespace schema_loader_tests {
class DocumentFiles {
  public:
    DocumentFiles()
        : root_{std::filesystem::temp_directory_path() /
                ("layout-document-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))} {
        std::filesystem::create_directories(root_);
        write("types.lispb", "");
        write("project.lispb", R"((lispb-project :language-version 1 :project-root "."
            (cpp-schema game :types "types.lispb" :sources ("game.lispb")
                :output-root (project-path "generated"))))");
        write("game.lispb", R"((module game :header "Game.h"
            (packed-value State :storage std::uint16_t
                (field health std::uint16_t :bits 8))))");
    }
    ~DocumentFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    auto path(std::string_view name) const -> std::filesystem::path { return root_ / name; }
    void write(std::string_view name, std::string_view contents) const {
        std::filesystem::create_directories(path(name).parent_path());
        std::ofstream output{path(name), std::ios::binary};
        output << contents;
    }
    auto read(std::string_view name) const -> std::string {
        std::ifstream input{path(name), std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
  private:
    std::filesystem::path root_;
};

TEST(DocumentFiles, SavesReloadsAndUndoesAcrossSaveBoundary) {
    DocumentFiles files;
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    EXPECT_FALSE(document.dirty());
    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    EXPECT_TRUE(document.dirty());
    ASSERT_TRUE(document.save());
    EXPECT_FALSE(document.dirty());
    auto reloaded{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_EQ(reloaded.query().module("game").packed_value("State").field("health").bits(), 12);
    ASSERT_TRUE(document.undo());
    EXPECT_TRUE(document.dirty());
    EXPECT_EQ(document.query().module("game").packed_value("State").field("health").bits(), 8);
    ASSERT_TRUE(document.save());
    ASSERT_TRUE(document.redo());
    EXPECT_TRUE(document.dirty());
    EXPECT_EQ(document.query().module("game").packed_value("State").field("health").bits(), 12);
}

TEST(DocumentFiles, DetectsExternalChangesWithoutDiscardingDraft) {
    DocumentFiles files;
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    auto const external{files.read("game.lispb") + "\n; external edit\n"};
    files.write("game.lispb", external);
    EXPECT_FALSE(document.save());
    EXPECT_EQ(files.read("game.lispb"), external);
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    EXPECT_EQ(document.query().module("game").packed_value("State").field("health").bits(), 12);
    EXPECT_GT(document.diagnostics().size(), document.last_diagnostic_begin());
}

TEST(DocumentFiles, SourceAdditionSharesSchemaHistoryAndPublishesProject) {
    DocumentFiles files;
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.add_source("extra.lispb", R"((module extra :header "Extra.h" (record Added)))");
    tx.add_member("extra", "Added", "value", "std::uint32_t");
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    EXPECT_TRUE(document.find_type("extra", "Added"));
    EXPECT_FALSE(std::filesystem::exists(files.path("extra.lispb")));
    ASSERT_TRUE(document.undo());
    EXPECT_FALSE(document.find_type("extra", "Added"));
    EXPECT_EQ(document.query().module("game").packed_value("State").field("health").bits(), 8);
    ASSERT_TRUE(document.redo());
    ASSERT_TRUE(document.save());
    EXPECT_TRUE(std::filesystem::exists(files.path("extra.lispb")));
    auto reloaded{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_TRUE(reloaded.find_type("extra", "Added"));
    EXPECT_NE(reloaded.declaration_source("extra", "Added")->find("std::uint32_t"),
              std::string::npos);
}

TEST(DocumentFiles, UnregistersSourceWithoutDeletingOriginalFile) {
    DocumentFiles files;
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto const original{files.read("game.lispb")};
    auto tx{document.transaction()};
    tx.add_source("replacement.lispb",
                  R"((module replacement :header "Replacement.h" (record New)))");
    tx.remove_source("game.lispb");
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    EXPECT_FALSE(document.find_type("game", "State"));
    ASSERT_TRUE(document.save());
    EXPECT_EQ(files.read("game.lispb"), original);
    auto reloaded{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_TRUE(reloaded.find_type("replacement", "New"));
    EXPECT_FALSE(reloaded.find_type("game", "State"));
}

TEST(DocumentFiles, RenamesSourceAndRetainsItsEditedContents) {
    DocumentFiles files;
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.rename_source("game.lispb", "schema/renamed.lispb");
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    ASSERT_TRUE(document.save());
    EXPECT_TRUE(std::filesystem::exists(files.path("schema/renamed.lispb")));
    EXPECT_FALSE(std::filesystem::exists(files.path("game.lispb")));
    auto reloaded{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_EQ(reloaded.query().module("game").packed_value("State").field("health").bits(), 12);
    ASSERT_TRUE(document.undo());
    ASSERT_TRUE(document.save());
    EXPECT_TRUE(std::filesystem::exists(files.path("game.lispb")));
    EXPECT_FALSE(std::filesystem::exists(files.path("schema/renamed.lispb")));
}

TEST(DocumentFiles, SavesInMemorySourcesAndRegistryAsIndependentProject) {
    DocumentFiles files;
    auto document{Document::from_source(
        R"((module game :header "Game.h" (record Entity (member value @external))))",
        R"((type external :spelling "External" :header "External.h"))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.rename_declaration("game", "Entity", "Saved");
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    ASSERT_TRUE(document.save_as(files.path("copy/project.lispb"), "copy"));
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    auto reloaded{Document::load(files.path("copy/project.lispb"), "copy")};
    ASSERT_TRUE(reloaded.valid()) << diagnostic_messages(reloaded);
    EXPECT_TRUE(reloaded.find_type("game", "Saved"));
    EXPECT_NE(files.read("copy/types.lispb").find("External.h"), std::string::npos);
    ASSERT_TRUE(document.undo());
    EXPECT_TRUE(document.dirty());
    ASSERT_TRUE(document.save());
    auto undone{Document::load(files.path("copy/project.lispb"), "copy")};
    ASSERT_TRUE(undone.valid()) << diagnostic_messages(undone);
    EXPECT_TRUE(undone.find_type("game", "Entity"));
}

TEST(DocumentFiles, InvalidLoadReportsAnOperationDiagnostic) {
    DocumentFiles files;
    auto missing_target{Document::load(files.path("project.lispb"), "missing")};
    EXPECT_FALSE(missing_target.valid());
    EXPECT_GT(missing_target.diagnostics().size(), 0);
    auto missing_file{Document::load(files.path("missing.lispb"), "game")};
    EXPECT_FALSE(missing_file.valid());
    EXPECT_GT(missing_file.diagnostics().size(), 0);
}

TEST(DocumentFiles, FailedStagingPreservesPublishedFilesAndDraftHistory) {
    DocumentFiles files;
    files.write("blocked", "existing file");
    auto const original_schema{files.read("game.lispb")};
    auto const original_project{files.read("project.lispb")};
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 12);
    tx.add_source("blocked/new.lispb", R"((module added :header "Added.h" (record New)))");
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    EXPECT_FALSE(document.save());
    EXPECT_EQ(files.read("game.lispb"), original_schema);
    EXPECT_EQ(files.read("project.lispb"), original_project);
    EXPECT_EQ(files.read("blocked"), "existing file");
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.find_type("added", "New"));
    EXPECT_GT(document.diagnostics().size(), document.last_diagnostic_begin());
    ASSERT_TRUE(document.undo());
    EXPECT_FALSE(document.find_type("added", "New"));
    EXPECT_FALSE(document.dirty());
}

TEST(DocumentFiles, VariantOnlyEditsLeaveSourcesClean) {
    DocumentFiles files;
    auto const original_schema{files.read("game.lispb")};
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.create_variant("Alternative");
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    EXPECT_FALSE(document.dirty());
    ASSERT_TRUE(document.save());
    EXPECT_EQ(files.read("game.lispb"), original_schema);
    ASSERT_TRUE(document.undo());
    EXPECT_FALSE(document.dirty());
}

TEST(DocumentFiles, LeavesUneditedFilesAndRegistryIncludesUntouched) {
    DocumentFiles files;
    files.write("types.lispb", R"((include "common/scalars.lispb"))");
    files.write("common/scalars.lispb", R"(; authored registry
        (type byte :spelling "std::uint8_t" (integer :signed false :bit-width 8)))");
    auto const registry{files.read("types.lispb")};
    auto const included{files.read("common/scalars.lispb")};
    auto document{Document::load(files.path("project.lispb"), "game")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(document);
    auto tx{document.transaction()};
    tx.set_field_bits("game", "State", "health", 12);
    ASSERT_TRUE(document.commit(tx)) << diagnostic_messages(document);
    ASSERT_TRUE(document.save());
    EXPECT_EQ(files.read("types.lispb"), registry);
    EXPECT_EQ(files.read("common/scalars.lispb"), included);
}
} // namespace
} // namespace ioj::layout
