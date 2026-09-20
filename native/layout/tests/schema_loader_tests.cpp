#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace ioj::layout {
namespace {

auto diagnostic_text(SchemaLoadResult const& loaded) -> std::string {
    std::string result;
    for (auto const& diagnostic : loaded.diagnostics) {
        result += diagnostic.message + '\n';
    }
    return result;
}

class TemporarySchemaProject {
  public:
    TemporarySchemaProject() {
        static int sequence{};
        root_ = std::filesystem::temp_directory_path() /
                ("layout-schema-clone-" + std::to_string(++sequence));
        std::filesystem::create_directories(root_ / "schema");
        write("project.lispb", R"((lispb-project
  :language-version 1
  :project-root "."
  (cpp-schema test-schema
    :types "schema/types.lispb"
    :sources ("schema/modules.lispb")
    :output-root (project-path "generated")))
)");
        write("schema/types.lispb", "");
        write("schema/modules.lispb", R"((enum-module states
  :header "States.h"
  :namespace test
  (enum State std::uint8_t
    (value Idle :value "0")))
)");
    }
    ~TemporarySchemaProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    auto path(std::filesystem::path const& relative) const -> std::filesystem::path {
        return root_ / relative;
    }
    auto read(std::filesystem::path const& relative) const -> std::string {
        std::ifstream input{path(relative), std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
  private:
    void write(std::filesystem::path const& relative, std::string_view const text) const {
        std::ofstream output{path(relative), std::ios::binary};
        output << text;
    }

    std::filesystem::path root_;
};

TEST(SchemaLoader, LoadsSemanticEnumsPackedValuesAndSoas) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};

    auto const entity_type{types.find_declared("native_entity_type", "EntityType")};
    auto const entity_id{types.find_declared("native_entity_unique_id", "EntityUniqueId")};
    auto const world_aabbs{types.find_declared("native_world_aabbs", "WorldAABBsColumns")};
    auto const vectors{types.find_declared("native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(entity_id.has_value());
    ASSERT_TRUE(world_aabbs.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& enumeration{std::get<lispb::schema::EnumType>(types.type(*entity_type).definition)};
    EXPECT_EQ(enumeration.enumerators.front().display_name, "Player Ship");

    auto const& packed{std::get<lispb::schema::PackedType>(types.type(*entity_id).definition)};
    ASSERT_EQ(packed.fields.size(), 2U);
    EXPECT_EQ(packed.fields[1].semantic_type.type, *entity_type);
    EXPECT_EQ(packed.invalid_raw_value, 0xffffffffU);

    auto const& soa{std::get<lispb::schema::SoaType>(types.type(*world_aabbs).definition)};
    ASSERT_EQ(soa.columns.size(), 6U);
    EXPECT_EQ(soa.related_storage_name, "WorldAABBs");

    auto const& vector_soa{std::get<lispb::schema::SoaType>(types.type(*vectors).definition)};
    ASSERT_EQ(vector_soa.columns.size(), 3U);
}

TEST(SchemaLoader, DerivesFactsThroughSemanticRepresentations) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};

    auto const history{types.find_declared("native_entity_history", "EntityHistoryColumns")};
    ASSERT_TRUE(history.has_value());
    auto const analysis{
        Analyzer::analyze_soa(types, *history, Variant{}, AbiProfile::host_common(), 1)};

    EXPECT_EQ(analysis.bytes_per_logical_element, 15);
    EXPECT_EQ(analysis.total_payload_bytes, 15);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(SchemaLoader, ReportsMissingProjectsWithoutThrowing) {
    auto const loaded{load_lispb_schema("missing/project.lispb", "sandbox-code")};
    EXPECT_FALSE(loaded.loaded);
    EXPECT_FALSE(loaded.diagnostics.empty());
}

TEST(SchemaLoader, ClonesCurrentDraftAndLoadsIndependentProject) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    auto const declaration{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "states",
                                           .namespace_name = "test",
                                           .name = "State"})};
    ASSERT_TRUE(declaration.has_value());
    auto edited{loaded.document->apply(
        lispb::schema::SetEnumeratorDisplayName{.enum_declaration = *declaration,
                                                .enumerator_name = "Idle",
                                                .display_name = "Idle State"})};
    ASSERT_TRUE(edited.has_value());
    ASSERT_TRUE(*edited);

    auto cloned{clone_lispb_schema(*loaded.document, files.path("copy.lispb"), "test-schema")};

    ASSERT_TRUE(cloned.loaded) << diagnostic_text(cloned);
    ASSERT_TRUE(cloned.document.has_value());
    EXPECT_FALSE(cloned.document->dirty());
    EXPECT_TRUE(std::filesystem::exists(files.path("copy_schema/types.lispb")));
    EXPECT_TRUE(std::filesystem::exists(files.path("copy_schema/module_1_modules.lispb")));
    auto const cloned_type{cloned.document->types().find_declared("states", "State")};
    ASSERT_TRUE(cloned_type.has_value());
    auto const& enumeration{
        std::get<lispb::schema::EnumType>(cloned.document->types().type(*cloned_type).definition)};
    EXPECT_EQ(enumeration.enumerators.front().display_name, "Idle State");
    EXPECT_EQ(files.read("schema/modules.lispb").find("Idle State"), std::string::npos);

    auto duplicate{clone_lispb_schema(*cloned.document, files.path("copy.lispb"), "test-schema")};
    EXPECT_FALSE(duplicate.loaded);
    EXPECT_FALSE(duplicate.diagnostics.empty());
    EXPECT_TRUE(std::filesystem::exists(files.path("copy_schema/types.lispb")));
}

} // namespace
} // namespace ioj::layout
