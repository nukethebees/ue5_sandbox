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
        write("schema/types.lispb", R"((type state
  :spelling "test::State")
)");
        write("schema/modules.lispb", R"((enum-module states
  :header "States.h"
  :namespace test
  (enum State std::uint8_t
    (value Idle :value "0")))

(packed-value-module packed
  :header "Packed.h"
  :namespace test
  (packed-value ExistingPacked
    :storage std::uint8_t
    (field value std::uint8_t :bits 8)))

(record-module records
  :header "Records.h"
  :namespace test
  (record ExistingRecord
    (member value std::uint32_t)))

(soa-module soa
  :header "Soa.h"
  :namespace test
  :backend standard-library
  (struct ExistingColumns
    (member values array std::uint32_t)))
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

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesPackedValue) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "packed",
                                           .namespace_name = "test",
                                           .name = "ExistingPacked"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{
        loaded.document->apply(
            lispb::schema::CreatePackedValue{
                .declaration = created,
                .module_index = loaded.document->declaration(*existing)->module_index,
                .schema =
                    codegen::PackedValueSchema{
                        .name = "DesignedId",
                        .storage_type = codegen::TypeRef{.name = "std::uint32_t",
                                                         .suffix = {},
                                                         .nested = std::nullopt},
                        .fields = {codegen::PackedFieldSchema{"entity_index",
                                                              codegen::TypeRef{
                                                                  .name = "std::uint32_t",
                                                                  .suffix = {},
                                                                  .nested = std::nullopt},
                                                              24},
                                   codegen::
                                       PackedFieldSchema{"state",
                                                         codegen::TypeRef{.name = "@state",
                                                                          .suffix = {},
                                                                          .nested = std::nullopt},
                                                         8,
                                                         codegen::PackedFieldKind::enumeration}},
                        .invalid_value = 0xffffffffU,
                        .export_specifier = std::nullopt},
                .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("packed", "DesignedId")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_packed(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common())};
    EXPECT_EQ(analysis.storage_bits, 32U);
    EXPECT_EQ(analysis.bits_used, 32U);
    EXPECT_EQ(analysis.unused_bits, 0U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_designed{reloaded.document->types().find_declared("packed", "DesignedId")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& packed{std::get<lispb::schema::PackedType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    ASSERT_EQ(packed.fields.size(), 2U);
    EXPECT_EQ(packed.fields[0].name, "entity_index");
    EXPECT_EQ(packed.fields[0].bit_width, 24U);
    EXPECT_EQ(packed.fields[1].name, "state");
    EXPECT_EQ(packed.fields[1].bit_width, 8U);
    EXPECT_EQ(reloaded.document->types().type(packed.fields[1].semantic_type.type).identity.name,
              "State");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_designed, Variant{}, AbiProfile::host_common());
    EXPECT_EQ(analysis.storage_bits, 32U);
    EXPECT_EQ(analysis.bits_used, 32U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesSoa) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "soa",
                                           .namespace_name = "test",
                                           .name = "ExistingColumns"})};
    ASSERT_TRUE(existing.has_value());

    codegen::SoaSchema schema{};
    schema.name = "DesignedColumns";
    schema.members = {
        codegen::SoaMemberSchema{
            .name = "states",
            .kind = codegen::SoaMemberKind::array,
            .type = codegen::TypeRef{.name = "@state", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {}},
        codegen::SoaMemberSchema{
            .name = "values",
            .kind = codegen::SoaMemberKind::array,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {}}};
    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateSoa{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema = std::move(schema),
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("soa", "DesignedColumns")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_soa(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.bytes_per_logical_element, 5U);
    EXPECT_EQ(analysis.total_payload_bytes, 500U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_designed{
        reloaded.document->types().find_declared("soa", "DesignedColumns")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& soa{std::get<lispb::schema::SoaType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    ASSERT_EQ(soa.columns.size(), 2U);
    EXPECT_EQ(soa.columns[0].name, "states");
    EXPECT_EQ(soa.columns[1].name, "values");
    EXPECT_EQ(reloaded.document->types().type(soa.columns[0].semantic_type.type).identity.name,
              "State");

    analysis = Analyzer::analyze_soa(
        reloaded.document->types(), *reloaded_designed, Variant{}, AbiProfile::host_common(), 100);
    EXPECT_EQ(analysis.bytes_per_logical_element, 5U);
    EXPECT_EQ(analysis.total_payload_bytes, 500U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesRecord) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "records",
                                           .namespace_name = "test",
                                           .name = "ExistingRecord"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateRecord{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema = codegen::RecordSchema{.name = "DesignedRecord",
                                        .members =
                                            {{.name = "state",
                                              .type = codegen::TypeRef{.name = "@state",
                                                                       .suffix = {},
                                                                       .nested = std::nullopt},
                                              .count = std::nullopt},
                                             {.name = "values",
                                              .type = codegen::TypeRef{.name = "std::uint32_t",
                                                                       .suffix = {},
                                                                       .nested = std::nullopt},
                                              .count = 2}},
                                        .export_specifier = std::nullopt},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("records", "DesignedRecord")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{
        Analyzer::analyze_record(loaded.document->types(), *designed, AbiProfile::host_common())};
    EXPECT_EQ(analysis.members[0].offset_bytes, 0);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[1].extent_bytes, 8);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_designed{
        reloaded.document->types().find_declared("records", "DesignedRecord")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& record{std::get<lispb::schema::RecordType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    ASSERT_EQ(record.members.size(), 2U);
    EXPECT_EQ(record.members[0].name, "state");
    EXPECT_EQ(reloaded.document->types().type(record.members[0].semantic_type.type).identity.name,
              "State");
    EXPECT_EQ(record.members[1].count, 2);

    analysis = Analyzer::analyze_record(
        reloaded.document->types(), *reloaded_designed, AbiProfile::host_common());
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
