#include <codegen/source_loader.h>
#include <lispb/project.h>
#include <lispb/schema/editable_document.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>

namespace lispb {
namespace {

class TemporaryProject {
  public:
    TemporaryProject() {
        static int sequence{};
        root_ = std::filesystem::temp_directory_path() /
                ("lispb-editable-project-test-" + std::to_string(++sequence));
        std::filesystem::create_directories(root_);
    }

    ~TemporaryProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void write(std::string const& name, std::string const& content) const {
        std::ofstream output{root_ / name, std::ios::binary | std::ios::trunc};
        output << content;
    }

    [[nodiscard]] auto path(std::string const& name) const -> std::filesystem::path {
        return root_ / name;
    }

    [[nodiscard]] auto read(std::string const& name) const -> std::string {
        std::ifstream input{root_ / name, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
  private:
    std::filesystem::path root_;
};

TEST(EditableProjectDocument, AddsValidatesPreviewsAndSavesCppSchemaSources) {
    TemporaryProject files;
    files.write("types.lispb", "");
    files.write("base.lispb", R"((module base
  :header "Base.h"
  :namespace test
  (enum Base
    (value First :value 0)))
)");
    files.write("extra.lispb", R"((module extra
  :header "Extra.h"
  :namespace test
  (integer-scalar Extra
    :signed false
    :minimum 0
    :maximum 15
    :bit-width auto))
)");
    files.write("invalid.lispb", "(not-a-module invalid)\n");
    files.write("project.lispb", R"((lispb-project
  :language-version 1
  :project-root "."

  ; Preserve this target and its source-list formatting.
  (cpp-schema test-schema
    :types "types.lispb"
    :sources (
      "base.lispb" ; keep this comment
    )
    :output-root (project-path "generated"))

  (group all
    :targets (test-schema)))
)");

    auto document{load_editable_project_document(files.path("project.lispb"))};
    ASSERT_FALSE(document.dirty());
    ASSERT_EQ(
        std::get<CppSchemaTarget>(document.project().targets.at("test-schema")).sources.size(), 1U);

    auto const original_revision{document.revision()};
    auto rejected{document.apply(
        AddCppSchemaSource{.target_name = "test-schema", .source = "missing.lispb"})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("does not exist"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    rejected =
        document.apply(AddCppSchemaSource{.target_name = "test-schema", .source = "base.lispb"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("already registered"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    rejected =
        document.apply(AddCppSchemaSource{.target_name = "test-schema", .source = "./base.lispb"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("already registered"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    rejected = document.apply(
        AddCppSchemaSource{.target_name = "test-schema", .source = "../outside.lispb"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("project root"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);

    rejected = document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "missing/authored.lispb", .contents = ""});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("parent directory"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    rejected = document.apply(
        AddCppSchemaSource{.target_name = "missing-target", .source = "extra.lispb"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("Unknown Lispb target"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);
    rejected =
        document.apply(AddCppSchemaSource{.target_name = "test-schema", .source = "invalid.lispb"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(document.revision(), original_revision);

    auto applied{
        document.apply(AddCppSchemaSource{.target_name = "test-schema", .source = "extra.lispb"})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    EXPECT_FALSE(document.can_redo());
    EXPECT_EQ(
        std::get<CppSchemaTarget>(document.project().targets.at("test-schema")).sources.size(), 2U);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().original, files.read("project.lispb"));
    EXPECT_NE(preview->front().updated.find("\"base.lispb\" ; keep this comment\n"
                                            "      \"extra.lispb\"\n"
                                            "    )"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("; Preserve this target"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("(group all\n    :targets (test-schema))"),
              std::string::npos);

    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.dirty());
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value());
    EXPECT_TRUE(preview->empty());
    ASSERT_TRUE(document.redo().value());
    EXPECT_TRUE(document.dirty());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_TRUE(*saved);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(std::filesystem::exists(files.path("project.lispb.layout-planner.tmp")));

    auto const project{load_project(files.path("project.lispb"))};
    auto const& target{std::get<CppSchemaTarget>(project.targets.at("test-schema"))};
    ASSERT_EQ(target.sources.size(), 2U);
    EXPECT_EQ(target.sources[1], "extra.lispb");
    std::vector<std::filesystem::path> sources;
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{codegen::load_sources(project.root / target.types, sources)};
    EXPECT_EQ(manifest.modules.size(), 2U);
    EXPECT_NE(files.read("project.lispb").find("; keep this comment"), std::string::npos);
}

TEST(EditableProjectDocument, CreatesValidatesPreviewsAndPublishesNewCppSchemaSource) {
    TemporaryProject files;
    files.write("types.lispb", "");
    files.write("base.lispb", R"((module base
  :header "Base.h"
  :namespace test
  (enum Base
    (value First :value 0)))
)");
    files.write("occupied.lispb", "; user-owned source\n");
    files.write("project.lispb", R"((lispb-project
  :language-version 1
  :project-root "."

  ; Preserve this comment and the original list item exactly.
  (cpp-schema test-schema
    :types "types.lispb"
    :sources (
      "base.lispb" ; original source
    )
    :output-root (project-path "generated")))
)");

    auto document{load_editable_project_document(files.path("project.lispb"))};
    auto const original_revision{document.revision()};
    auto const original_project{files.read("project.lispb")};

    auto rejected{document.apply(CreateCppSchemaSource{.target_name = "test-schema",
                                                       .source = "invalid.lispb",
                                                       .contents = "(not-a-module invalid)\n"})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(std::filesystem::exists(files.path("invalid.lispb")));

    rejected = document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "collision.lispb", .contents = R"((module base
  :header "Collision.h"
  :namespace test
  (enum Base
    (value Other :value 1)))
)"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_FALSE(std::filesystem::exists(files.path("collision.lispb")));

    rejected = document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "occupied.lispb", .contents = ""});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("already exists"), std::string::npos);
    EXPECT_EQ(files.read("occupied.lispb"), "; user-owned source\n");
    EXPECT_EQ(document.revision(), original_revision);

    rejected = document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "../outside.lispb", .contents = ""});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("project root"), std::string::npos);
    EXPECT_EQ(document.revision(), original_revision);

    auto created{document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "authored.lispb", .contents = ""})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);
    EXPECT_TRUE(document.dirty());
    EXPECT_TRUE(document.can_undo());
    EXPECT_FALSE(std::filesystem::exists(files.path("authored.lispb")));

    auto const created_revision{document.revision()};
    rejected = document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "./authored.lispb", .contents = ""});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE(rejected.error().message.find("already registered"), std::string::npos);
    EXPECT_EQ(document.revision(), created_revision);

    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    auto const manifest_update{
        std::ranges::find(*preview, files.path("project.lispb"), &ProjectSourceUpdate::path)};
    ASSERT_NE(manifest_update, preview->end());
    EXPECT_EQ(manifest_update->original, original_project);
    EXPECT_NE(manifest_update->updated.find("\"base.lispb\" ; original source\n"
                                            "      \"authored.lispb\"\n"
                                            "    )"),
              std::string::npos);
    EXPECT_NE(manifest_update->updated.find("; Preserve this comment"), std::string::npos);
    auto const source_update{
        std::ranges::find(*preview, files.path("authored.lispb"), &ProjectSourceUpdate::path)};
    ASSERT_NE(source_update, preview->end());
    EXPECT_TRUE(source_update->original.empty());
    EXPECT_TRUE(source_update->updated.empty());

    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(document.preview_source_updates()->empty());
    EXPECT_FALSE(std::filesystem::exists(files.path("authored.lispb")));
    ASSERT_TRUE(document.redo().value());
    EXPECT_TRUE(document.dirty());

    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_TRUE(*saved);
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(std::filesystem::is_regular_file(files.path("authored.lispb")));
    EXPECT_TRUE(files.read("authored.lispb").empty());
    EXPECT_FALSE(std::filesystem::exists(files.path("authored.lispb.layout-planner.tmp")));
    EXPECT_FALSE(std::filesystem::exists(files.path("project.lispb.layout-planner.tmp")));
    EXPECT_NE(files.read("project.lispb").find("; original source"), std::string::npos);

    auto const project{load_project(files.path("project.lispb"))};
    auto const& target{std::get<CppSchemaTarget>(project.targets.at("test-schema"))};
    ASSERT_EQ(target.sources.size(), 2U);
    EXPECT_EQ(target.sources[1], "authored.lispb");
    std::vector<std::filesystem::path> sources;
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto schema_document{
        schema::load_editable_schema_document(project.root / target.types, sources)};
    auto const authored_source{std::ranges::find(schema_document.source_files(),
                                                 files.path("authored.lispb"),
                                                 &schema::SchemaSourceFile::path)};
    ASSERT_NE(authored_source, schema_document.source_files().end());
    auto const source_index{
        static_cast<std::size_t>(authored_source - schema_document.source_files().begin())};
    auto module_created{schema_document.apply(
        schema::CreateModule{.source_file_index = source_index,
                             .schema = codegen::NormalModuleSchema{
                                 .settings = codegen::ModuleSettings{.name = "authored_scalars",
                                                                     .header = "AuthoredScalars.h",
                                                                     .namespace_name = "test"}}})};
    ASSERT_TRUE(module_created.has_value()) << module_created.error().message;
    ASSERT_TRUE(*module_created);
    auto module_preview{schema_document.preview_source_updates()};
    ASSERT_TRUE(module_preview.has_value()) << module_preview.error().message;
    ASSERT_EQ(module_preview->size(), 1U);
    EXPECT_EQ(module_preview->front().path, files.path("authored.lispb"));
    EXPECT_NE(module_preview->front().updated.find("(module authored_scalars"), std::string::npos);
}

TEST(EditableProjectDocument, ValidatesMultiplePendingSourcesAndCleansFailedPublication) {
    TemporaryProject files;
    files.write("types.lispb", "");
    files.write("base.lispb", R"((module base
  :header "Base.h"
  (enum Base
    (value First :value 0)))
)");
    files.write("project.lispb", R"((lispb-project
  :language-version 1
  :project-root "."
  (cpp-schema test-schema
    :types "types.lispb"
    :sources ("base.lispb")
    :output-root (project-path "generated")))
)");
    auto const original_project{files.read("project.lispb")};
    auto document{load_editable_project_document(files.path("project.lispb"))};

    ASSERT_TRUE(document
                    .apply(CreateCppSchemaSource{
                        .target_name = "test-schema", .source = "first.lispb", .contents = ""})
                    .has_value());
    auto second{document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "second.lispb", .contents = R"((module values
  :header "Values.h"
  (integer-scalar Value
    :signed false
    :minimum 0
    :maximum 3
    :bit-width auto))
)"})};
    ASSERT_TRUE(second.has_value()) << second.error().message;
    ASSERT_TRUE(*second);
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    EXPECT_EQ(preview->size(), 3U);

    ASSERT_TRUE(document.undo().value());
    EXPECT_EQ(document.preview_source_updates()->size(), 2U);
    ASSERT_TRUE(document.redo().value());
    EXPECT_EQ(document.preview_source_updates()->size(), 3U);

    files.write("second.lispb", "; appeared after the draft was created\n");
    auto failed_save{document.save()};
    ASSERT_FALSE(failed_save.has_value());
    EXPECT_NE(failed_save.error().message.find("already exists"), std::string::npos);
    EXPECT_TRUE(document.dirty());
    EXPECT_FALSE(std::filesystem::exists(files.path("first.lispb")));
    EXPECT_EQ(files.read("second.lispb"), "; appeared after the draft was created\n");
    EXPECT_EQ(files.read("project.lispb"), original_project);
    EXPECT_FALSE(std::filesystem::exists(files.path("first.lispb.layout-planner.tmp")));
    EXPECT_FALSE(std::filesystem::exists(files.path("second.lispb.layout-planner.tmp")));
    EXPECT_FALSE(std::filesystem::exists(files.path("project.lispb.layout-planner.tmp")));

    ASSERT_TRUE(std::filesystem::remove(files.path("second.lispb")));
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_TRUE(*saved);
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(std::filesystem::is_regular_file(files.path("first.lispb")));
    EXPECT_NE(files.read("second.lispb").find("(module values"), std::string::npos);

    auto const project{load_project(files.path("project.lispb"))};
    auto const& target{std::get<CppSchemaTarget>(project.targets.at("test-schema"))};
    ASSERT_EQ(target.sources.size(), 3U);
    std::vector<std::filesystem::path> sources;
    for (auto const& source : target.sources) {
        sources.push_back(project.root / source);
    }
    auto const manifest{codegen::load_sources(project.root / target.types, sources)};
    EXPECT_EQ(manifest.modules.size(), 2U);
}

TEST(EditableProjectDocument, UnregistersOriginalSourceWithoutDeletingIt) {
    TemporaryProject files;
    files.write("types.lispb", "");
    files.write("base.lispb", R"((module base
  :header "Base.h"
  :namespace test
  (integer-scalar EntityTable
    :signed false
    :minimum 0
    :maximum 1023
    :bit-width 10))
)");
    files.write("dependent.lispb", R"((module dependent
  :header "Dependent.h"
  :namespace test
  (integer-scalar EntityIndex
    :signed false
    :minimum 0
    :maximum 1023
    :bit-width auto
    (relation index_into test::EntityTable)))
)");
    files.write("independent.lispb", "; independent source remains on disk\n");
    files.write("project.lispb", R"((lispb-project
  :language-version 1
  :project-root "."
  (cpp-schema test-schema
    :types "types.lispb"
    :sources (
      "base.lispb" ; base source note
      "dependent.lispb" ; dependent source note
      "independent.lispb" ; independent source note
    )
    :output-root (project-path "generated")))
)");
    auto const original_project{files.read("project.lispb")};
    auto document{load_editable_project_document(files.path("project.lispb"))};
    auto const original_revision{document.revision()};

    auto rejected{document.apply(
        RemoveCppSchemaSource{.target_name = "test-schema", .source = "base.lispb"})};
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(document.revision(), original_revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(files.read("project.lispb"), original_project);

    auto removed{document.apply(
        RemoveCppSchemaSource{.target_name = "test-schema", .source = "independent.lispb"})};
    ASSERT_TRUE(removed.has_value()) << removed.error().message;
    ASSERT_TRUE(*removed);
    EXPECT_TRUE(document.dirty());
    auto preview{document.preview_source_updates()};
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 1U);
    EXPECT_EQ(preview->front().original, original_project);
    EXPECT_EQ(preview->front().updated.find("\"independent.lispb\""), std::string::npos);
    EXPECT_NE(preview->front().updated.find("; independent source note"), std::string::npos);
    EXPECT_NE(preview->front().updated.find("\"base.lispb\" ; base source note"),
              std::string::npos);
    EXPECT_NE(preview->front().updated.find("\"dependent.lispb\" ; dependent source note"),
              std::string::npos);

    auto created{document.apply(CreateCppSchemaSource{
        .target_name = "test-schema", .source = "authored.lispb", .contents = {}})};
    ASSERT_TRUE(created.has_value()) << created.error().message;
    ASSERT_TRUE(*created);
    preview = document.preview_source_updates();
    ASSERT_TRUE(preview.has_value()) << preview.error().message;
    ASSERT_EQ(preview->size(), 2U);
    EXPECT_NE(preview->front().updated.find("\"authored.lispb\""), std::string::npos);
    EXPECT_EQ(preview->front().updated.find("\"independent.lispb\""), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(files.path("authored.lispb")));

    ASSERT_TRUE(document.undo().value());
    ASSERT_TRUE(document.undo().value());
    EXPECT_FALSE(document.dirty());
    EXPECT_TRUE(document.preview_source_updates()->empty());
    ASSERT_TRUE(document.redo().value());
    EXPECT_TRUE(document.dirty());

    auto restored{document.apply(
        AddCppSchemaSource{.target_name = "test-schema", .source = "independent.lispb"})};
    ASSERT_TRUE(restored.has_value()) << restored.error().message;
    ASSERT_TRUE(*restored);
    EXPECT_TRUE(document.preview_source_updates()->empty());
    auto no_op_save{document.save()};
    ASSERT_TRUE(no_op_save.has_value()) << no_op_save.error().message;
    EXPECT_FALSE(*no_op_save);
    EXPECT_FALSE(document.dirty());

    ASSERT_TRUE(document.undo().value());
    EXPECT_TRUE(document.dirty());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_TRUE(*saved);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(files.read("independent.lispb"), "; independent source remains on disk\n");
    EXPECT_TRUE(std::filesystem::is_regular_file(files.path("independent.lispb")));

    auto const reloaded{load_editable_project_document(files.path("project.lispb"))};
    auto const& target{std::get<CppSchemaTarget>(reloaded.project().targets.at("test-schema"))};
    ASSERT_EQ(target.sources.size(), 2U);
    EXPECT_EQ(target.sources[0], "base.lispb");
    EXPECT_EQ(target.sources[1], "dependent.lispb");
    EXPECT_NE(files.read("project.lispb").find("; independent source note"), std::string::npos);
}

} // namespace
} // namespace lispb
