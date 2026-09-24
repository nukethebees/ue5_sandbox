#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/schema_loader.hpp>

#include "../lib/src/schema_loader_transaction.hpp"

#include <gtest/gtest.h>

#include <cmath>
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
        write("schema/modules.lispb", R"((module states
  :header "States.h"
  :namespace test
  (enum State std::uint8_t
    (value Idle :value "0")))

(module packed
  :header "Packed.h"
  :namespace test
  (packed-value ExistingPacked
    :storage std::uint8_t
    (field value std::uint8_t :bits 8)))

(module scalars
  :header "Scalars.h"
  :namespace test
  (integer-scalar Health
    :signed false
    :minimum 0
    :maximum 1000
    :bit-width auto
    (code Invalid :value 1023 :sentinel true))
  (integer-scalar Temperature
    :signed true
    :minimum -100
    :maximum 100
    :bit-width auto))

(module representations
  :header "Representations.h"
  :namespace test
  (linear-quantized ExistingHealthQ8
    :source test::Health
    :bits 8
    :reserved-codes 0
    :clipping reject)
  (fixed-point ExistingVelocityQ3_4
    :signed true
    :total-bits 8
    :fractional-bits 4
    :rounding toward-zero)
  (mini-float ExistingMiniF8
    :sign-bits 1
    :exponent-bits 3
    :significand-bits 4
    :bias 3))

(module records
  :header "Records.h"
  :namespace test
  (record ExistingRecord
    (member value std::uint32_t)))

(module soa
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
    void write_source(std::filesystem::path const& relative, std::string_view const text) const {
        std::filesystem::create_directories(path(relative).parent_path());
        write(relative, text);
    }
  private:
    void write(std::filesystem::path const& relative, std::string_view const text) const {
        std::ofstream output{path(relative), std::ios::binary};
        output << text;
    }

    std::filesystem::path root_;
};

TEST(SchemaLoader, SavesReloadsAndClonesIncludedExternalScalars) {
    TemporarySchemaProject files;
    auto const registry{std::string{R"((include "common/scalars.lispb"))"}};
    auto const common{std::string{R"(; Shared scalar contracts remain source-authored.
      (type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))"}};
    files.write_source("schema/types.lispb", registry);
    files.write_source("schema/common/scalars.lispb", common);
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    auto& document{*loaded.document};
    ASSERT_EQ(document.source_files().size(), 3);
    EXPECT_EQ(document.source_files()[1].kind, lispb::schema::SchemaSourceKind::type_registry);
    EXPECT_EQ(document.source_files()[2].kind, lispb::schema::SchemaSourceKind::module);
    auto const source{document.registered_type_source("signed_byte")};
    ASSERT_TRUE(source.has_value());
    EXPECT_EQ(source->source_file_index, 1);
    EXPECT_EQ(source->line, 2);
    auto const record{*document.types().find_declared("records", "ExistingRecord")};
    auto const id{*document.find_declaration(document.types().type(record).identity)};
    ASSERT_TRUE(
        document.apply(lispb::schema::RenameDeclaration{.declaration = id, .new_name = "Renamed"})
            .has_value());
    ASSERT_TRUE(document.undo().has_value());
    ASSERT_TRUE(document.redo().has_value());
    auto const saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_EQ(files.read("schema/types.lispb"), registry);
    EXPECT_EQ(files.read("schema/common/scalars.lispb"), common);
    EXPECT_TRUE(document.types().find_declared("records", "Renamed").has_value());
    ASSERT_NE(lispb::schema::integer_domain(
                  document.types().type(*document.types().find_registered("signed_byte"))),
              nullptr);

    auto const cloned{clone_lispb_schema(document, files.path("copy.lispb"), "test-schema")};
    ASSERT_TRUE(cloned.loaded) << diagnostic_text(cloned);
    ASSERT_TRUE(cloned.document.has_value());
    EXPECT_EQ(cloned.document->source_files().size(), 3);
    EXPECT_EQ(cloned.document->manifest().modules.size(), document.manifest().modules.size());
    auto const cloned_source{cloned.document->registered_type_source("signed_byte")};
    ASSERT_TRUE(cloned_source.has_value());
    EXPECT_NE(cloned.document->source_files()[cloned_source->source_file_index].path,
              document.source_files()[source->source_file_index].path);
    files.write_source("schema/common/scalars.lispb", "(invalid original registry)");
    auto const independent{load_lispb_schema(files.path("copy.lispb"), "test-schema")};
    EXPECT_TRUE(independent.loaded) << diagnostic_text(independent);
}

TEST(SchemaLoader, ExternalScalarRepresentationsAnalyzeWithoutInventingAbiFacts) {
    TemporarySchemaProject files;
    files.write_source("schema/types.lispb", R"(
      (type index :spelling "NativeIndex" (integer :signed false :bit-width 8 :maximum 254
        (code invalid :value 255 :sentinel true)))
      (type real :spelling "double" (floating-point :format ieee754-binary64)))");
    files.write_source("schema/modules.lispb", R"((module reps :header "Reps.h"
      (linear-quantized Q :source @index :bits 4)
      (integer-varint V :source @index :encoding unsigned)
      (optional-sentinel S :source @index :sentinel invalid)
      (optional-presence-bit P :source @index)
      (record Values (member value @index))))");
    auto const loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    auto const& types{loaded.document->types()};
    auto const source{*types.find_registered("index")};
    auto const capabilities{declaration_capabilities(types.type(source))};
    EXPECT_EQ(capabilities.kind, DeclarationKind::external);
    EXPECT_TRUE(capabilities.inspectable);
    EXPECT_FALSE(capabilities.editable);
    EXPECT_FALSE(capabilities.supports_variant_overrides);
    EXPECT_EQ(integer_source_reference(types.type(source)), "@index");
    EXPECT_THROW(Analyzer::analyze_integer_scalar(types, *types.find_registered("real")),
                 std::invalid_argument);
    auto const q{Analyzer::analyze_linear_quantized(types, *types.find_declared("reps", "Q"))};
    EXPECT_EQ(q.source_minimum, codegen::PackedIntegerValue{0});
    EXPECT_EQ(q.source_maximum, codegen::PackedIntegerValue{254});
    EXPECT_EQ(q.encoded_storage_bits, 4);
    auto const v{Analyzer::analyze_integer_varint(types, *types.find_declared("reps", "V"))};
    EXPECT_EQ(v.minimum_encoded_bytes, 1);
    EXPECT_EQ(v.maximum_encoded_bytes, 2);
    auto const s{Analyzer::analyze_optional_sentinel(types, *types.find_declared("reps", "S"))};
    EXPECT_EQ(s.sentinel_value, codegen::PackedIntegerValue{255});
    EXPECT_EQ(s.encoded_storage_bits, 8);
    auto const p{Analyzer::analyze_optional_presence_bit(types, *types.find_declared("reps", "P"))};
    EXPECT_EQ(p.encoded_storage_bits, 9);
    auto const record{*types.find_declared("reps", "Values")};
    PlannerAnalysisSession session{types};
    session.inputs.selection.select_type(types, record);
    session.refresh(&*loaded.document);
    ASSERT_TRUE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.results().record_analysis->size_bytes.has_value());
    session.inputs.selection.select_type(types, source);
    session.refresh(&*loaded.document);
    EXPECT_FALSE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.primary_abi().find("NativeIndex").has_value());
    EXPECT_EQ(lispb::schema::integer_domain(types.type(source))->bit_width, 8);
}

TEST(SchemaLoader, LoadsSemanticEnumsPackedValuesAndSoas) {
    auto const project_path{std::filesystem::path{SANDBOX_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.project_document.has_value());
    EXPECT_EQ(loaded.project_document->path(), std::filesystem::absolute(project_path));
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
    ASSERT_EQ(packed.segments.size(), 2U);
    EXPECT_EQ(std::get<lispb::schema::PackedField>(packed.segments[1]).semantic_type.type,
              *entity_type);
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

TEST(SchemaLoader, CreatesBlankProjectAndReopensCleanDocument) {
    TemporarySchemaProject files;
    auto created{create_blank_lispb_schema(files.path("blank.lispb"), "blank-schema")};
    ASSERT_TRUE(created.loaded) << diagnostic_text(created);
    ASSERT_TRUE(created.document.has_value());
    EXPECT_FALSE(created.document->dirty());
    EXPECT_TRUE(std::filesystem::exists(files.path("blank_schema/types.lispb")));
    EXPECT_TRUE(std::filesystem::exists(files.path("blank_schema/source.lispb")));
    EXPECT_FALSE(std::filesystem::exists(files.path("blank.lispb.layout-planner.tmp")));

    auto reopened{load_lispb_schema(files.path("blank.lispb"), "blank-schema")};
    ASSERT_TRUE(reopened.loaded) << diagnostic_text(reopened);
    ASSERT_TRUE(reopened.document.has_value());
    EXPECT_FALSE(reopened.document->dirty());
}

TEST(SchemaLoader, RejectsInvalidBlankDestinationsWithoutPublishingFiles) {
    TemporarySchemaProject files;
    auto check_failure = [&](std::filesystem::path const& destination, std::string const& target) {
        auto created{create_blank_lispb_schema(destination, target)};
        EXPECT_FALSE(created.loaded);
        EXPECT_FALSE(created.diagnostics.empty());
    };

    check_failure(files.path("bad.txt"), "blank-schema");
    check_failure(files.path("blank.lispb"), "bad target");
    check_failure(files.path("missing/blank.lispb"), "blank-schema");
    EXPECT_FALSE(std::filesystem::exists(files.path("bad_schema")));
    EXPECT_FALSE(std::filesystem::exists(files.path("blank_schema")));
    EXPECT_FALSE(std::filesystem::exists(files.path("missing")));

    check_failure(files.path("project.lispb"), "blank-schema");
    EXPECT_TRUE(std::filesystem::exists(files.path("project.lispb")));
    std::filesystem::create_directory(files.path("blank_schema"));
    check_failure(files.path("blank.lispb"), "blank-schema");
    EXPECT_TRUE(std::filesystem::is_directory(files.path("blank_schema")));
    std::filesystem::remove(files.path("blank_schema"));
    {
        std::ofstream temporary{files.path("blank.lispb.layout-planner.tmp")};
        temporary << "owned elsewhere";
    }
    check_failure(files.path("blank.lispb"), "blank-schema");
    EXPECT_FALSE(std::filesystem::exists(files.path("blank_schema")));
    EXPECT_FALSE(std::filesystem::exists(files.path("blank.lispb")));
    EXPECT_EQ(files.read("blank.lispb.layout-planner.tmp"), "owned elsewhere");
}

TEST(SchemaLoader, CloneUsesTheSameDestinationValidationAsBlankCreation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());

    for (auto const& destination : {files.path("bad.txt"), files.path("missing/copy.lispb")}) {
        auto cloned{clone_lispb_schema(*loaded.document, destination, "test-schema")};
        EXPECT_FALSE(cloned.loaded);
        EXPECT_FALSE(cloned.diagnostics.empty());
    }
    auto invalid_target{
        clone_lispb_schema(*loaded.document, files.path("copy.lispb"), "bad target")};
    EXPECT_FALSE(invalid_target.loaded);
    EXPECT_FALSE(std::filesystem::exists(files.path("copy_schema")));
    EXPECT_FALSE(std::filesystem::exists(files.path("copy.lispb")));
}

TEST(SchemaLoader, CloneRollsBackPublishedFilesIfFinalReopenFails) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    int open_count{};
    auto const cloned{detail::clone_lispb_schema_with_opener(
        *loaded.document,
        files.path("failed.lispb"),
        "test-schema",
        [&](std::filesystem::path const& path, std::string const& target) {
            ++open_count;
            if (open_count == 2) {
                SchemaLoadResult failure;
                failure.diagnostics.push_back(
                    {DiagnosticSeverity::error, "forced final reopen failure"});
                return failure;
            }
            return load_lispb_schema(path, target);
        })};
    EXPECT_EQ(open_count, 2);
    EXPECT_FALSE(cloned.loaded);
    EXPECT_FALSE(std::filesystem::exists(files.path("failed.lispb")));
    EXPECT_FALSE(std::filesystem::exists(files.path("failed_schema")));
    EXPECT_FALSE(std::filesystem::exists(files.path("failed.lispb.layout-planner.tmp")));
}

TEST(SchemaLoader, DanglingSymlinkDestinationsAreTreatedAsOccupied) {
    TemporarySchemaProject files;
    std::error_code error;
    std::filesystem::create_symlink(files.path("missing-target"), files.path("blank.lispb"), error);
    if (error) {
        GTEST_SKIP() << "Creating symlinks is unavailable: " << error.message();
    }
    auto const blank{create_blank_lispb_schema(files.path("blank.lispb"), "test-schema")};
    EXPECT_FALSE(blank.loaded);
    EXPECT_FALSE(std::filesystem::exists(files.path("blank_schema")));
    EXPECT_TRUE(
        std::filesystem::is_symlink(std::filesystem::symlink_status(files.path("blank.lispb"))));

    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    auto const cloned{
        clone_lispb_schema(*loaded.document, files.path("blank.lispb"), "test-schema")};
    EXPECT_FALSE(cloned.loaded);
    EXPECT_FALSE(std::filesystem::exists(files.path("blank_schema")));
    EXPECT_TRUE(
        std::filesystem::is_symlink(std::filesystem::symlink_status(files.path("blank.lispb"))));
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
    ASSERT_TRUE(cloned.project_document.has_value());
    EXPECT_EQ(cloned.project_document->path(), std::filesystem::absolute(files.path("copy.lispb")));
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
    auto applied{loaded.document->apply(lispb::schema::CreatePackedValue{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema =
            codegen::PackedValueSchema{.name = "DesignedId",
                                       .storage_type = codegen::TypeRef{.name = "std::uint32_t",
                                                                        .suffix = {},
                                                                        .nested = std::nullopt},
                                       .segments = {codegen::PackedFieldSchema{.name =
                                                                                   "entity_index",
                                                                               .type = codegen::TypeRef{.name =
                                                                                                            "std::uint32_t",
                                                                                                        .suffix = {},
                                                                                                        .nested = std::
                                                                                                            nullopt},
                                                                               .bits = 24,
                                                                               .kind =
                                                                                   codegen::
                                                                                       PackedFieldKind::
                                                                                           unsigned_integer,
                                                                               .range_helper =
                                                                                   false,
                                                                               .minimum_value =
                                                                                   std::nullopt,
                                                                               .maximum_value =
                                                                                   std::nullopt,
                                                                               .named_codes = {},
                                                                               .relationship =
                                                                                   std::nullopt},
                                                    codegen::PackedFieldSchema{.name = "state",
                                                                               .type =
                                                                                   codegen::TypeRef{.name = "@st"
                                                                                                            "at"
                                                                                                            "e",
                                                                                                    .suffix = {},
                                                                                                    .nested = std::nullopt},
                                                                               .bits = 8,
                                                                               .kind = codegen::
                                                                                   PackedFieldKind::enumeration,
                                                                               .range_helper =
                                                                                   false,
                                                                               .minimum_value =
                                                                                   std::nullopt,
                                                                               .maximum_value =
                                                                                   std::nullopt,
                                                                               .named_codes = {},
                                                                               .relationship =
                                                                                   std::nullopt}},
                                       .invalid_value = 0xffffffffU,
                                       .export_specifier = std::nullopt,
                                       .byte_order = std::nullopt,
                                       .bit_order = std::nullopt},
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
    ASSERT_EQ(packed.segments.size(), 2U);
    auto const& index{std::get<lispb::schema::PackedField>(packed.segments[0])};
    auto const& state{std::get<lispb::schema::PackedField>(packed.segments[1])};
    EXPECT_EQ(index.name, "entity_index");
    EXPECT_EQ(index.bit_width, 24U);
    EXPECT_EQ(state.name, "state");
    EXPECT_EQ(state.bit_width, 8U);
    EXPECT_EQ(reloaded.document->types().type(state.semantic_type.type).identity.name, "State");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_designed, Variant{}, AbiProfile::host_common());
    EXPECT_EQ(analysis.storage_bits, 32U);
    EXPECT_EQ(analysis.bits_used, 32U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(SchemaLoader, PlacesSavesReloadsAndAnalyzesLinearQuantizedPackedField) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const packed_declaration{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "packed",
                                           .namespace_name = "test",
                                           .name = "ExistingPacked"})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{*loaded.document->packed_value_schema(*packed_declaration)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type.name = "test::ExistingHealthQ8";
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::linear_quantized;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    field.relationship.reset();
    auto applied{loaded.document->apply(lispb::schema::ReplacePackedValue{
        .declaration = *packed_declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_packed(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common())};
    ASSERT_EQ(analysis.fields.size(), 1U);
    ASSERT_TRUE(analysis.fields.front().linear_quantized.has_value());
    EXPECT_EQ(analysis.fields.front().bit_width, 8U);
    EXPECT_EQ(analysis.fields.front().linear_quantized->source_maximum, 1000U);
    EXPECT_EQ(analysis.fields.front().linear_quantized->usable_code_count,
              (ExactCodeCount{.value = 256, .two_to_64 = false}));
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_packed{
        reloaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{std::get<lispb::schema::PackedType>(
        reloaded.document->types().type(*reloaded_packed).definition)};
    auto const& reloaded_field{std::get<lispb::schema::PackedField>(packed.segments.front())};
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded.document->types().type(reloaded_field.semantic_type.type).identity.name,
              "ExistingHealthQ8");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_packed, Variant{}, AbiProfile::host_common());
    ASSERT_TRUE(analysis.fields.front().linear_quantized.has_value());
    EXPECT_EQ(analysis.fields.front().linear_quantized->source_minimum, 0U);
    EXPECT_EQ(analysis.fields.front().linear_quantized->source_maximum, 1000U);
}

TEST(SchemaLoader, PlacesSavesReloadsAndAnalyzesFixedPointPackedField) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const packed_declaration{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "packed",
                                           .namespace_name = "test",
                                           .name = "ExistingPacked"})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{*loaded.document->packed_value_schema(*packed_declaration)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type.name = "test::ExistingVelocityQ3_4";
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::fixed_point;
    field.range_helper = false;
    field.minimum_value.reset();
    field.maximum_value.reset();
    field.named_codes.clear();
    field.relationship.reset();
    auto applied{loaded.document->apply(lispb::schema::ReplacePackedValue{
        .declaration = *packed_declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_packed(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common())};
    ASSERT_EQ(analysis.fields.size(), 1U);
    ASSERT_TRUE(analysis.fields.front().fixed_point.has_value());
    EXPECT_EQ(analysis.fields.front().bit_width, 8U);
    EXPECT_EQ(analysis.fields.front().fixed_point->minimum_raw_value,
              codegen::PackedIntegerValue{-128});
    EXPECT_EQ(analysis.fields.front().fixed_point->maximum_raw_value,
              codegen::PackedIntegerValue{127});
    EXPECT_EQ(analysis.fields.front().fixed_point->resolution, 0.0625L);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_packed{
        reloaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{std::get<lispb::schema::PackedType>(
        reloaded.document->types().type(*reloaded_packed).definition)};
    auto const& reloaded_field{std::get<lispb::schema::PackedField>(packed.segments.front())};
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::fixed_point);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded.document->types().type(reloaded_field.semantic_type.type).identity.name,
              "ExistingVelocityQ3_4");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_packed, Variant{}, AbiProfile::host_common());
    ASSERT_TRUE(analysis.fields.front().fixed_point.has_value());
    EXPECT_TRUE(analysis.fields.front().fixed_point->signedness);
    EXPECT_EQ(analysis.fields.front().fixed_point->fractional_bits, 4U);
    EXPECT_EQ(analysis.fields.front().fixed_point->rounding,
              codegen::FixedPointRounding::toward_zero);
}

TEST(SchemaLoader, PlacesSavesReloadsAndAnalyzesMiniFloatPackedField) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const packed_declaration{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "packed",
                                           .namespace_name = "test",
                                           .name = "ExistingPacked"})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{*loaded.document->packed_value_schema(*packed_declaration)};
    auto& field{std::get<codegen::PackedFieldSchema>(replacement.segments.front())};
    field.type.name = "test::ExistingMiniF8";
    field.bits.reset();
    field.kind = codegen::PackedFieldKind::mini_float;
    auto applied{loaded.document->apply(lispb::schema::ReplacePackedValue{
        .declaration = *packed_declaration, .schema = std::move(replacement)})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_packed(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common())};
    ASSERT_EQ(analysis.fields.size(), 1U);
    ASSERT_TRUE(analysis.fields.front().mini_float.has_value());
    EXPECT_EQ(analysis.fields.front().bit_width, 8U);
    EXPECT_EQ(analysis.fields.front().mini_float->sign_bits, 1U);
    EXPECT_EQ(analysis.fields.front().mini_float->exponent_bits, 3U);
    EXPECT_EQ(analysis.fields.front().mini_float->significand_bits, 4U);
    EXPECT_TRUE(analysis.diagnostics.empty());

    Variant overridden;
    overridden.overrides.packed_field_widths[{.type = *designed, .field_name = "value"}] = 7;
    auto const ignored_override{Analyzer::analyze_packed(
        loaded.document->types(), *designed, overridden, AbiProfile::host_common())};
    EXPECT_EQ(ignored_override.fields.front().bit_width, 8U);
    EXPECT_FALSE(ignored_override.fields.front().overridden);
    ASSERT_EQ(ignored_override.diagnostics.size(), 1U);
    EXPECT_EQ(ignored_override.diagnostics.front().severity, DiagnosticSeverity::warning);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_packed{
        reloaded.document->types().find_declared("packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{std::get<lispb::schema::PackedType>(
        reloaded.document->types().type(*reloaded_packed).definition)};
    auto const& reloaded_field{std::get<lispb::schema::PackedField>(packed.segments.front())};
    EXPECT_EQ(reloaded_field.kind, codegen::PackedFieldKind::mini_float);
    EXPECT_EQ(reloaded_field.bit_width, 8U);
    EXPECT_TRUE(reloaded_field.bit_width_auto);
    EXPECT_EQ(reloaded.document->types().type(reloaded_field.semantic_type.type).identity.name,
              "ExistingMiniF8");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_packed, Variant{}, AbiProfile::host_common());
    ASSERT_TRUE(analysis.fields.front().mini_float.has_value());
    EXPECT_EQ(analysis.fields.front().mini_float->exponent_bias, 3);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesLinearQuantizedRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateLinearQuantized{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema =
            codegen::LinearQuantizedSchema{.name = "TemperatureQ10",
                                           .source = codegen::TypeRef{.name = "test::Temperature",
                                                                      .suffix = {},
                                                                      .nested = std::nullopt},
                                           .bit_width = 10,
                                           .reserved_codes = 1,
                                           .clipping = codegen::QuantizationClipping::clamp},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{
        loaded.document->types().find_declared("representations", "TemperatureQ10")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_linear_quantized(loaded.document->types(), *designed)};
    EXPECT_EQ(analysis.encoded_storage_bits, 10U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 1024, .two_to_64 = false}));
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 1023, .two_to_64 = false}));
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_EQ(analysis.source_span, 200U);
    EXPECT_NEAR(static_cast<double>(analysis.resolution), 200.0 / 1022.0, 1e-12);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "TemperatureQ10")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& quantized{std::get<lispb::schema::LinearQuantizedType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_EQ(quantized.bit_width, 10U);
    EXPECT_EQ(quantized.reserved_codes, 1U);
    EXPECT_EQ(quantized.clipping, codegen::QuantizationClipping::clamp);
    EXPECT_EQ(reloaded.document->types().type(quantized.source.type).identity.name, "Temperature");

    analysis = Analyzer::analyze_linear_quantized(reloaded.document->types(), *reloaded_designed);
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 1023, .two_to_64 = false}));
    EXPECT_EQ(analysis.source_minimum, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(analysis.source_maximum, codegen::PackedIntegerValue{100});
    EXPECT_NEAR(static_cast<double>(analysis.maximum_rounding_error), 100.0 / 1022.0, 1e-12);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesIntegerVarintRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateIntegerVarint{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema =
            codegen::IntegerVarintSchema{
                .name = "HealthVarint",
                .source =
                    codegen::TypeRef{.name = "test::Health", .suffix = {}, .nested = std::nullopt},
                .encoding = codegen::IntegerVarintEncoding::unsigned_varint},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("representations", "HealthVarint")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_integer_varint(loaded.document->types(), *designed, 100)};
    EXPECT_EQ(analysis.minimum_encoded_bytes, 1U);
    EXPECT_EQ(analysis.maximum_encoded_bytes, 2U);
    EXPECT_EQ(analysis.minimum_total_bytes, 100U);
    EXPECT_EQ(analysis.maximum_total_bytes, 200U);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "HealthVarint")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& varint{std::get<lispb::schema::IntegerVarintType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_EQ(varint.encoding, codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_EQ(reloaded.document->types().type(varint.source.type).identity.name, "Health");

    analysis =
        Analyzer::analyze_integer_varint(reloaded.document->types(), *reloaded_designed, 100);
    EXPECT_EQ(analysis.minimum_total_bytes, 100U);
    EXPECT_EQ(analysis.maximum_total_bytes, 200U);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesFixedPointRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateFixedPoint{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema = codegen::FixedPointSchema{.name = "VelocityQ8_8",
                                            .signedness = true,
                                            .total_bits = 16,
                                            .fractional_bits = 8,
                                            .rounding = codegen::FixedPointRounding::nearest_even},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("representations", "VelocityQ8_8")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_fixed_point(loaded.document->types(), *designed, 100)};
    EXPECT_EQ(analysis.total_encoded_bits, 1'600U);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.resolution), 1.0 / 256.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.minimum_value), -128.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_value), 127.0 + 255.0 / 256.0);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "VelocityQ8_8")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& fixed{std::get<lispb::schema::FixedPointType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_TRUE(fixed.signedness);
    EXPECT_EQ(fixed.total_bits, 16U);
    EXPECT_EQ(fixed.fractional_bits, 8U);
    EXPECT_EQ(fixed.rounding, codegen::FixedPointRounding::nearest_even);

    analysis = Analyzer::analyze_fixed_point(reloaded.document->types(), *reloaded_designed, 100);
    EXPECT_EQ(analysis.total_encoded_bits, 1'600U);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 1.0 / 512.0);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesMiniFloatRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateMiniFloat{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema = codegen::MiniFloatSchema{.name = "CompactFloat",
                                           .sign_bits = 1,
                                           .exponent_bits = 5,
                                           .significand_bits = 10,
                                           .exponent_bias = 15},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("representations", "CompactFloat")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_mini_float(loaded.document->types(), *designed, 100)};
    EXPECT_EQ(analysis.total_bits, 16U);
    EXPECT_EQ(analysis.total_encoded_bits, 1'600U);
    EXPECT_EQ(analysis.nan_code_count, 2'046U);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.maximum_finite), 65'504.0);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "CompactFloat")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& mini_float{std::get<lispb::schema::MiniFloatType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_EQ(mini_float.sign_bits, 1U);
    EXPECT_EQ(mini_float.exponent_bits, 5U);
    EXPECT_EQ(mini_float.significand_bits, 10U);
    EXPECT_EQ(mini_float.exponent_bias, 15);

    analysis = Analyzer::analyze_mini_float(reloaded.document->types(), *reloaded_designed, 100);
    EXPECT_EQ(analysis.total_encoded_bits, 1'600U);
    EXPECT_DOUBLE_EQ(static_cast<double>(*analysis.minimum_positive_subnormal),
                     std::ldexp(1.0, -24));
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesOptionalSentinelRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateOptionalSentinel{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema =
            codegen::OptionalSentinelSchema{
                .name = "OptionalHealth",
                .source =
                    codegen::TypeRef{.name = "test::Health", .suffix = {}, .nested = std::nullopt},
                .sentinel = "Invalid"},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{
        loaded.document->types().find_declared("representations", "OptionalHealth")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_optional_sentinel(loaded.document->types(), *designed, 1'000)};
    EXPECT_EQ(analysis.present_value_count, 1'001U);
    EXPECT_EQ(analysis.absence_code_count, 1U);
    EXPECT_EQ(analysis.other_sentinel_code_count, 0U);
    EXPECT_EQ(analysis.unused_code_count, 22U);
    EXPECT_EQ(analysis.encoded_storage_bits, 10U);
    EXPECT_EQ(analysis.total_encoded_bits, 10'000U);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "OptionalHealth")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& optional{std::get<lispb::schema::OptionalSentinelType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_EQ(optional.sentinel_name, "Invalid");
    EXPECT_EQ(optional.sentinel_value, codegen::PackedIntegerValue{1023});
    EXPECT_EQ(optional.bit_width, 10U);
    EXPECT_EQ(reloaded.document->types().type(optional.source.type).identity.name, "Health");

    analysis =
        Analyzer::analyze_optional_sentinel(reloaded.document->types(), *reloaded_designed, 1'000);
    EXPECT_EQ(analysis.total_encoded_bits, 10'000U);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesOptionalPresenceBitRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{
        loaded.document->find_declaration({.origin = lispb::schema::TypeOrigin::declaration,
                                           .module_name = "representations",
                                           .namespace_name = "test",
                                           .name = "ExistingHealthQ8"})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(lispb::schema::CreateOptionalPresenceBit{
        .declaration = created,
        .module_index = loaded.document->declaration(*existing)->module_index,
        .schema =
            codegen::OptionalPresenceBitSchema{
                .name = "PresentHealth",
                .source =
                    codegen::TypeRef{.name = "test::Health", .suffix = {}, .nested = std::nullopt}},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("representations", "PresentHealth")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{
        Analyzer::analyze_optional_presence_bit(loaded.document->types(), *designed, 1'000)};
    EXPECT_EQ(analysis.present_value_count, 1'001U);
    EXPECT_EQ(analysis.canonical_absence_state_count, 1U);
    EXPECT_EQ(analysis.source_sentinel_code_count, 1U);
    EXPECT_EQ(analysis.source_unused_payload_codes, 22U);
    EXPECT_EQ(analysis.payload_bits, 10U);
    EXPECT_EQ(analysis.encoded_storage_bits, 11U);
    EXPECT_EQ(analysis.total_encoded_bits, 11'000U);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        reloaded.document->types().find_declared("representations", "PresentHealth")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& optional{std::get<lispb::schema::OptionalPresenceBitType>(
        reloaded.document->types().type(*reloaded_designed).definition)};
    EXPECT_EQ(optional.payload_bits, 10U);
    EXPECT_EQ(optional.encoded_bits, 11U);
    EXPECT_EQ(reloaded.document->types().type(optional.source.type).identity.name, "Health");

    analysis = Analyzer::analyze_optional_presence_bit(
        reloaded.document->types(), *reloaded_designed, 1'000);
    EXPECT_EQ(analysis.total_encoded_bits, 11'000U);
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
            .mask_dimensions = {},
            .relationship = std::nullopt},
        codegen::SoaMemberSchema{
            .name = "values",
            .kind = codegen::SoaMemberKind::array,
            .type = codegen::TypeRef{.name = "std::uint32_t", .suffix = {}, .nested = std::nullopt},
            .fixed_schema = std::nullopt,
            .nested_schema = std::nullopt,
            .mask_field = false,
            .mask_dimensions = {},
            .relationship = codegen::SemanticRelationSchema{
                .kind = codegen::SemanticRelationKind::references,
                .target = codegen::TypeRef{.name = "ExistingColumns",
                                           .suffix = {},
                                           .nested = std::nullopt},
                .unit = std::nullopt}}};
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
    auto const& designed_soa{
        std::get<lispb::schema::SoaType>(loaded.document->types().type(*designed).definition)};
    ASSERT_TRUE(designed_soa.columns[1].relationship.has_value());
    EXPECT_EQ(loaded.document->types()
                  .type(designed_soa.columns[1].relationship->target.type)
                  .identity.name,
              "ExistingColumns");

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
    ASSERT_TRUE(soa.columns[1].relationship.has_value());
    EXPECT_EQ(
        reloaded.document->types().type(soa.columns[1].relationship->target.type).identity.name,
        "ExistingColumns");

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
        .schema =
            codegen::RecordSchema{.name = "DesignedRecord",
                                  .members =
                                      {{.name = "state",
                                        .type = codegen::TypeRef{.name = "@state",
                                                                 .suffix = {},
                                                                 .nested = std::nullopt},
                                        .count =
                                            std::nullopt,
                                        .relationship = codegen::
                                            SemanticRelationSchema{.kind = codegen::SemanticRelationKind::references,
                                                                   .target = codegen::TypeRef{.name =
                                                                                                  "ExistingRecord",
                                                                                              .suffix = {},
                                                                                              .nested =
                                                                                                  std::nullopt},
                                                                   .unit = std::nullopt}},
                                       {.name = "values",
                                        .type =
                                            codegen::TypeRef{
                                                .name = "std::uint32_t",
                                                .suffix = {},
                                                .nested = std::nullopt},
                                        .count = 2,
                                        .relationship = std::nullopt}},
                                  .export_specifier = std::nullopt},
        .insertion_index = std::nullopt})};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{loaded.document->types().find_declared("records", "DesignedRecord")};
    auto const existing_type{loaded.document->types().find_declared("records", "ExistingRecord")};
    ASSERT_TRUE(designed.has_value());
    ASSERT_TRUE(existing_type.has_value());
    auto const& designed_record{
        std::get<lispb::schema::RecordType>(loaded.document->types().type(*designed).definition)};
    ASSERT_TRUE(designed_record.members[0].relationship.has_value());
    EXPECT_EQ(designed_record.members[0].relationship->target.type, *existing_type);
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
    ASSERT_TRUE(record.members[0].relationship.has_value());
    auto const reloaded_existing{
        reloaded.document->types().find_declared("records", "ExistingRecord")};
    ASSERT_TRUE(reloaded_existing.has_value());
    EXPECT_EQ(record.members[0].relationship->target.type, *reloaded_existing);
    EXPECT_EQ(record.members[1].count, 2);

    analysis = Analyzer::analyze_record(
        reloaded.document->types(), *reloaded_designed, AbiProfile::host_common());
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
