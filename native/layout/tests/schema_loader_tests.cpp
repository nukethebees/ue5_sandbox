#include "../lib/src/schema_loader_transaction.hpp"
#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_session.hpp>
#include <ioj/layout/schema_loader.hpp>

#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/sexpr/source_file_kind.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_origin.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    EXPECT_EQ(document.storage().sources.kind(document.source_files()[1]),
              lispb::codegen::sexpr::SourceFileKind::type_registry);
    EXPECT_EQ(document.storage().sources.kind(document.source_files()[2]),
              lispb::codegen::sexpr::SourceFileKind::module);
    auto const source{document.registered_type_source("signed_byte")};
    ASSERT_TRUE(source.has_value());
    EXPECT_EQ(document.source_span(*source).file, document.source_files()[1]);
    EXPECT_EQ(document.source_location(*source).line, 2);
    auto const record{*fixture_find(document.types(), "records", "ExistingRecord")};
    auto const id{*document.find_declaration(document.types().identity(record))};
    ASSERT_TRUE(document.apply(document.rename_declaration(id, "Renamed")).has_value());
    ASSERT_TRUE(document.undo().has_value());
    ASSERT_TRUE(document.redo().has_value());
    auto const saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_EQ(files.read("schema/types.lispb"), registry);
    EXPECT_EQ(files.read("schema/common/scalars.lispb"), common);
    EXPECT_TRUE(fixture_find(document.types(), "records", "Renamed").has_value());
    ASSERT_TRUE(lispb::schema::integer_domain(document.types(),
                                              *document.types().find_registered(
                                                  document.types().strings().find("signed_byte")))
                    .has_value());

    auto const cloned{clone_lispb_schema(document, files.path("copy.lispb"), "test-schema")};
    ASSERT_TRUE(cloned.loaded) << diagnostic_text(cloned);
    ASSERT_TRUE(cloned.document.has_value());
    EXPECT_EQ(cloned.document->source_files().size(), 3);
    EXPECT_EQ(cloned.document->manifest().modules.size(), document.manifest().modules.size());
    auto const cloned_source{cloned.document->registered_type_source("signed_byte")};
    ASSERT_TRUE(cloned_source.has_value());
    EXPECT_NE(cloned.document->storage().sources.filesystem_path(
                  cloned.document->source_span(*cloned_source).file),
              document.storage().sources.filesystem_path(document.source_span(*source).file));
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
    auto const source{*types.find_registered(types.strings().find("index"))};
    auto const capabilities{declaration_capabilities(types, source)};
    EXPECT_EQ(capabilities.kind, DeclarationKind::external);
    EXPECT_TRUE(capabilities.inspectable);
    EXPECT_FALSE(capabilities.editable);
    EXPECT_FALSE(capabilities.supports_variant_overrides);
    EXPECT_EQ(integer_source_reference(types, source), "@index");
    EXPECT_THROW(Analyzer::analyze_integer_scalar(
                     types, *types.find_registered(types.strings().find("real"))),
                 std::invalid_argument);
    auto const q{Analyzer::analyze_linear_quantized(types, *fixture_find(types, "reps", "Q"))};
    EXPECT_EQ(q.source_minimum, lispb::codegen::PackedIntegerValue{0});
    EXPECT_EQ(q.source_maximum, lispb::codegen::PackedIntegerValue{254});
    EXPECT_EQ(q.encoded_storage_bits, 4);
    auto const v{Analyzer::analyze_integer_varint(types, *fixture_find(types, "reps", "V"))};
    EXPECT_EQ(v.minimum_encoded_bytes, 1);
    EXPECT_EQ(v.maximum_encoded_bytes, 2);
    auto const s{Analyzer::analyze_optional_sentinel(types, *fixture_find(types, "reps", "S"))};
    EXPECT_EQ(s.sentinel_value, lispb::codegen::PackedIntegerValue{255});
    EXPECT_EQ(s.encoded_storage_bits, 8);
    auto const p{Analyzer::analyze_optional_presence_bit(types, *fixture_find(types, "reps", "P"))};
    EXPECT_EQ(p.encoded_storage_bits, 9);
    auto const record{*fixture_find(types, "reps", "Values")};
    PlannerAnalysisSession session{types};
    session.inputs.selection.select_type(types, record);
    session.refresh(&*loaded.document);
    ASSERT_TRUE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.results().record_analysis->size_bytes.has_value());
    session.inputs.selection.select_type(types, source);
    session.refresh(&*loaded.document);
    EXPECT_FALSE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.primary_abi().find("NativeIndex").has_value());
    auto const domain{lispb::schema::integer_domain(types, source)};
    ASSERT_TRUE(domain.has_value());
    EXPECT_EQ(types.definitions()
                  .integer_scalars.bit_width[types.definitions().integer_scalars.index(*domain)],
              8);
}

TEST(SchemaLoader, LoadsSemanticEnumsPackedValuesAndSoas) {
    auto const project_path{std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.project_document.has_value());
    EXPECT_EQ(loaded.project_document->path(), std::filesystem::absolute(project_path));
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};

    auto const entity_type{fixture_find(types, "native_entity_type", "EntityType")};
    auto const entity_id{fixture_find(types, "native_entity_unique_id", "EntityUniqueId")};
    auto const world_aabbs{fixture_find(types, "native_world_aabbs", "WorldAABBsColumns")};
    auto const vectors{fixture_find(types, "native_vectors_3f", "Vectors3f")};
    ASSERT_TRUE(entity_type.has_value());
    ASSERT_TRUE(entity_id.has_value());
    ASSERT_TRUE(world_aabbs.has_value());
    ASSERT_TRUE(vectors.has_value());

    auto const& enumeration{types.definition(*entity_type)};
    EXPECT_EQ(
        types.strings().view(
            *types.declarations().enumerators.display_name[types.declarations().enumerators.index(
                types.enum_domains()
                    .values(types.definitions()
                                .enums.domain[types.definitions().enums.index(enumeration)])
                    .front())]),
        "Player Ship");

    auto const& packed{types.definition(*entity_id)};
    ASSERT_EQ(types.definitions().packed.segments_of(packed).size(), 2U);
    EXPECT_EQ(
        types.occurrences().type[types.occurrences().index(
            types.definitions().packed_fields.semantic_type[types.definitions().packed_fields.index(
                types.definitions().packed.segments_of(packed)[1])])],
        *entity_type);
    EXPECT_EQ(
        types.definitions().packed.invalid_raw_value[types.definitions().packed.index(packed)],
        0xffffffffU);

    auto const& soa{types.definition(*world_aabbs)};
    ASSERT_EQ(types.definitions().soas.columns_of(soa).size(), 6U);
    ASSERT_TRUE(types.definitions().soas.related_storage_name[types.definitions().soas.index(soa)]);
    EXPECT_EQ(
        types.cpp_types().strings().view(
            *types.definitions().soas.related_storage_name[types.definitions().soas.index(soa)]),
        "WorldAABBs");

    auto const& vector_soa{types.definition(*vectors)};
    ASSERT_EQ(types.definitions().soas.columns_of(vector_soa).size(), 3U);
}

TEST(SchemaLoader, AnalyzesComposedFlightModelAndCollisionRecords) {
    auto const loaded{load_lispb_schema(
        std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb", "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    auto const& graph{loaded.document->types()};
    auto abi{AbiProfile::host_common()};
    abi.set("Vector3f",
            {.size_bytes = 12,
             .alignment_bytes = 4,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "Core Vector<float, 3> contract",
             .origin = FactOrigin::manual_assumption});
    abi.set("ioj::sim::collision::CellCoord",
            {.size_bytes = sizeof(int) * 3,
             .alignment_bytes = alignof(int),
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "Core Vector<int, 3> contract",
             .origin = FactOrigin::manual_assumption});
    for (auto const& [module, name] :
         {std::pair{"native_player_flight_model_data", "FlightModelLoadout"},
          std::pair{"native_collision_grid_geometry_grid_geometry", "GridGeometry"},
          std::pair{"native_collision_grid_geometry_cell_coord_bounds", "CellCoordBounds"},
          std::pair{"native_collision_world_aabb", "WorldAABB"}}) {
        auto const id{fixture_find(graph, module, name)};
        ASSERT_TRUE(id.has_value()) << name;
        auto const analysis{Analyzer::analyze_record(graph, *id, abi, 1)};
        EXPECT_TRUE(analysis.diagnostics.empty()) << name;
    }
}

TEST(SchemaLoader, DerivesFactsThroughSemanticRepresentations) {
    auto const project_path{std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb"};
    auto const loaded{load_lispb_schema(project_path, "sandbox-code")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const& types{loaded.document->types()};

    auto const history{fixture_find(types, "native_entity_history", "EntityHistoryColumns")};
    ASSERT_TRUE(history.has_value());
    auto abi{AbiProfile::host_common()};
    abi.set("SimTick",
            {.size_bytes = sizeof(std::uint64_t),
             .alignment_bytes = alignof(std::uint64_t),
             .integer_signed = false,
             .unsigned_value_bits = 64,
             .provenance = "Core SimTick uint64 contract",
             .origin = FactOrigin::manual_assumption});
    abi.set("Vector3f",
            {.size_bytes = 12,
             .alignment_bytes = 4,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "Core Vector<float, 3> contract",
             .origin = FactOrigin::manual_assumption});
    auto const analysis{Analyzer::analyze_soa(types, *history, Variant{}, abi, 1)};

    EXPECT_EQ(analysis.bytes_per_logical_element, 35);
    EXPECT_EQ(analysis.total_payload_bytes, 35);
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
    auto const declaration{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("states"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("State")})};
    ASSERT_TRUE(declaration.has_value());
    auto edited{loaded.document->apply(
        loaded.document->set_enumerator_display_name(*declaration, "Idle", "Idle State"))};
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
    auto const cloned_type{fixture_find(cloned.document->types(), "states", "State")};
    ASSERT_TRUE(cloned_type.has_value());
    auto const& enumeration{cloned.document->types().definition(*cloned_type)};
    EXPECT_EQ(
        cloned.document->storage().strings.view(
            *cloned.document->types()
                 .declarations()
                 .enumerators
                 .display_name[cloned.document->types().declarations().enumerators.index(
                     cloned.document->types()
                         .enum_domains()
                         .values(
                             cloned.document->types().definitions().enums.domain
                                 [cloned.document->types().definitions().enums.index(enumeration)])
                         .front())]),
        "Idle State");
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
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("packed"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingPacked")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((packed-value DesignedId :storage std::uint32_t :invalid-value 4294967295 (field entity_index std::uint32_t :bits 24) (field state @state :bits 8 :kind enum)))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "packed", "DesignedId")};
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
    auto const reloaded_designed{fixture_find(reloaded.document->types(), "packed", "DesignedId")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& packed{reloaded.document->types().definition(*reloaded_designed)};
    ASSERT_EQ(reloaded.document->types().definitions().packed.segments_of(packed).size(), 2U);
    auto const& index{reloaded.document->types().definitions().packed.segments_of(packed)[0]};
    auto const& state{reloaded.document->types().definitions().packed.segments_of(packed)[1]};
    EXPECT_EQ(reloaded.document->storage().strings.view(
                  reloaded.document->types()
                      .definitions()
                      .packed_fields
                      .name[reloaded.document->types().definitions().packed_fields.index(index)]),
              "entity_index");
    EXPECT_EQ(reloaded.document->types()
                  .definitions()
                  .packed_fields
                  .bit_width[reloaded.document->types().definitions().packed_fields.index(index)],
              24U);
    EXPECT_EQ(reloaded.document->storage().strings.view(
                  reloaded.document->types()
                      .definitions()
                      .packed_fields
                      .name[reloaded.document->types().definitions().packed_fields.index(state)]),
              "state");
    EXPECT_EQ(reloaded.document->types()
                  .definitions()
                  .packed_fields
                  .bit_width[reloaded.document->types().definitions().packed_fields.index(state)],
              8U);
    EXPECT_EQ(
        reloaded.document->types().strings().view(reloaded.document->types().name(
            reloaded.document->types()
                .occurrences()
                .type[reloaded.document->types().occurrences().index(
                    reloaded.document->types().definitions().packed_fields.semantic_type
                        [reloaded.document->types().definitions().packed_fields.index(state)])])),
        "State");

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
    auto const packed_declaration{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("packed"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingPacked")})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{loaded.document->storage().declarations.packed_values.clone(
        *loaded.document->declaration_handle(*packed_declaration,
                                             lispb::codegen::DeclarationKind::packed_value))};
    auto& declarations{loaded.document->storage().declarations};
    loaded.document->storage()
        .declarations.packed_values
        .segments[loaded.document->storage().declarations.packed_values.index(replacement)] = [&] {
        auto const original_items{declarations.items(
            loaded.document->storage().declarations.packed_values.segments
                [loaded.document->storage().declarations.packed_values.index(replacement)])};
        std::vector<lispb::codegen::PackedSegmentId> segments{original_items.begin(),
                                                              original_items.end()};
        segments.front() = [&] {
            auto field{declarations.packed_fields.clone(segments.front())};
            declarations.packed_fields.type[declarations.packed_fields.index(field)] =
                fixture_reference(loaded.document->storage(), "test::ExistingHealthQ8");
            declarations.packed_fields.bits[declarations.packed_fields.index(field)].reset();
            declarations.packed_fields.kind[declarations.packed_fields.index(field)] =
                lispb::codegen::PackedFieldKind::linear_quantized;
            declarations.packed_fields.range_helper[declarations.packed_fields.index(field)] =
                false;
            declarations.packed_fields.minimum_value[declarations.packed_fields.index(field)]
                .reset();
            declarations.packed_fields.maximum_value[declarations.packed_fields.index(field)]
                .reset();
            declarations.packed_fields.named_codes[declarations.packed_fields.index(field)] = {};
            declarations.packed_fields.relationship[declarations.packed_fields.index(field)]
                .reset();
            return field;
        }();
        return declarations.range(segments);
    }();
    auto applied{loaded.document->apply(
        loaded.document->replace_declaration(*packed_declaration, replacement))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "packed", "ExistingPacked")};
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
        fixture_find(reloaded.document->types(), "packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{reloaded.document->types().definition(*reloaded_packed)};
    auto const& reloaded_field{
        reloaded.document->types().definitions().packed.segments_of(packed).front()};
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .packed_fields
            .kind[reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
        lispb::codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(reloaded.document->types().definitions().packed_fields.bit_width
                  [reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
              8U);
    EXPECT_TRUE(reloaded.document->types().definitions().packed_fields.bit_width_auto
                    [reloaded.document->types().definitions().packed_fields.index(reloaded_field)]);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().packed_fields.semantic_type
                              [reloaded.document->types().definitions().packed_fields.index(
                                  reloaded_field)])])),
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
    auto const packed_declaration{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("packed"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingPacked")})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{loaded.document->storage().declarations.packed_values.clone(
        *loaded.document->declaration_handle(*packed_declaration,
                                             lispb::codegen::DeclarationKind::packed_value))};
    auto& declarations{loaded.document->storage().declarations};
    loaded.document->storage()
        .declarations.packed_values
        .segments[loaded.document->storage().declarations.packed_values.index(replacement)] = [&] {
        auto const original_items{declarations.items(
            loaded.document->storage().declarations.packed_values.segments
                [loaded.document->storage().declarations.packed_values.index(replacement)])};
        std::vector<lispb::codegen::PackedSegmentId> segments{original_items.begin(),
                                                              original_items.end()};
        segments.front() = [&] {
            auto field{declarations.packed_fields.clone(segments.front())};
            declarations.packed_fields.type[declarations.packed_fields.index(field)] =
                fixture_reference(loaded.document->storage(), "test::ExistingVelocityQ3_4");
            declarations.packed_fields.bits[declarations.packed_fields.index(field)].reset();
            declarations.packed_fields.kind[declarations.packed_fields.index(field)] =
                lispb::codegen::PackedFieldKind::fixed_point;
            declarations.packed_fields.range_helper[declarations.packed_fields.index(field)] =
                false;
            declarations.packed_fields.minimum_value[declarations.packed_fields.index(field)]
                .reset();
            declarations.packed_fields.maximum_value[declarations.packed_fields.index(field)]
                .reset();
            declarations.packed_fields.named_codes[declarations.packed_fields.index(field)] = {};
            declarations.packed_fields.relationship[declarations.packed_fields.index(field)]
                .reset();
            return field;
        }();
        return declarations.range(segments);
    }();
    auto applied{loaded.document->apply(
        loaded.document->replace_declaration(*packed_declaration, replacement))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "packed", "ExistingPacked")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_packed(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common())};
    ASSERT_EQ(analysis.fields.size(), 1U);
    ASSERT_TRUE(analysis.fields.front().fixed_point.has_value());
    EXPECT_EQ(analysis.fields.front().bit_width, 8U);
    EXPECT_EQ(analysis.fields.front().fixed_point->minimum_raw_value,
              lispb::codegen::PackedIntegerValue{-128});
    EXPECT_EQ(analysis.fields.front().fixed_point->maximum_raw_value,
              lispb::codegen::PackedIntegerValue{127});
    EXPECT_EQ(analysis.fields.front().fixed_point->resolution, 0.0625L);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_packed{
        fixture_find(reloaded.document->types(), "packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{reloaded.document->types().definition(*reloaded_packed)};
    auto const& reloaded_field{
        reloaded.document->types().definitions().packed.segments_of(packed).front()};
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .packed_fields
            .kind[reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
        lispb::codegen::PackedFieldKind::fixed_point);
    EXPECT_EQ(reloaded.document->types().definitions().packed_fields.bit_width
                  [reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
              8U);
    EXPECT_TRUE(reloaded.document->types().definitions().packed_fields.bit_width_auto
                    [reloaded.document->types().definitions().packed_fields.index(reloaded_field)]);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().packed_fields.semantic_type
                              [reloaded.document->types().definitions().packed_fields.index(
                                  reloaded_field)])])),
              "ExistingVelocityQ3_4");

    analysis = Analyzer::analyze_packed(
        reloaded.document->types(), *reloaded_packed, Variant{}, AbiProfile::host_common());
    ASSERT_TRUE(analysis.fields.front().fixed_point.has_value());
    EXPECT_TRUE(analysis.fields.front().fixed_point->signedness);
    EXPECT_EQ(analysis.fields.front().fixed_point->fractional_bits, 4U);
    EXPECT_EQ(analysis.fields.front().fixed_point->rounding,
              lispb::codegen::FixedPointRounding::toward_zero);
}

TEST(SchemaLoader, PlacesSavesReloadsAndAnalyzesMiniFloatPackedField) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const packed_declaration{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("packed"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingPacked")})};
    ASSERT_TRUE(packed_declaration.has_value());

    auto replacement{loaded.document->storage().declarations.packed_values.clone(
        *loaded.document->declaration_handle(*packed_declaration,
                                             lispb::codegen::DeclarationKind::packed_value))};
    auto& declarations{loaded.document->storage().declarations};
    loaded.document->storage()
        .declarations.packed_values
        .segments[loaded.document->storage().declarations.packed_values.index(replacement)] = [&] {
        auto const original_items{declarations.items(
            loaded.document->storage().declarations.packed_values.segments
                [loaded.document->storage().declarations.packed_values.index(replacement)])};
        std::vector<lispb::codegen::PackedSegmentId> segments{original_items.begin(),
                                                              original_items.end()};
        segments.front() = [&] {
            auto field{declarations.packed_fields.clone(segments.front())};
            declarations.packed_fields.type[declarations.packed_fields.index(field)] =
                fixture_reference(loaded.document->storage(), "test::ExistingMiniF8");
            declarations.packed_fields.bits[declarations.packed_fields.index(field)].reset();
            declarations.packed_fields.kind[declarations.packed_fields.index(field)] =
                lispb::codegen::PackedFieldKind::mini_float;
            return field;
        }();
        return declarations.range(segments);
    }();
    auto applied{loaded.document->apply(
        loaded.document->replace_declaration(*packed_declaration, replacement))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "packed", "ExistingPacked")};
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
        fixture_find(reloaded.document->types(), "packed", "ExistingPacked")};
    ASSERT_TRUE(reloaded_packed.has_value());
    auto const& packed{reloaded.document->types().definition(*reloaded_packed)};
    auto const& reloaded_field{
        reloaded.document->types().definitions().packed.segments_of(packed).front()};
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .packed_fields
            .kind[reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
        lispb::codegen::PackedFieldKind::mini_float);
    EXPECT_EQ(reloaded.document->types().definitions().packed_fields.bit_width
                  [reloaded.document->types().definitions().packed_fields.index(reloaded_field)],
              8U);
    EXPECT_TRUE(reloaded.document->types().definitions().packed_fields.bit_width_auto
                    [reloaded.document->types().definitions().packed_fields.index(reloaded_field)]);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().packed_fields.semantic_type
                              [reloaded.document->types().definitions().packed_fields.index(
                                  reloaded_field)])])),
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
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((linear-quantized TemperatureQ10 :source test::Temperature :bits 10 :reserved-codes 1 :clipping clamp))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{
        fixture_find(loaded.document->types(), "representations", "TemperatureQ10")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_linear_quantized(loaded.document->types(), *designed)};
    EXPECT_EQ(analysis.encoded_storage_bits, 10U);
    EXPECT_EQ(analysis.total_code_count, (ExactCodeCount{.value = 1024, .two_to_64 = false}));
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 1023, .two_to_64 = false}));
    EXPECT_EQ(analysis.source_minimum, lispb::codegen::PackedIntegerValue{-100});
    EXPECT_EQ(analysis.source_maximum, lispb::codegen::PackedIntegerValue{100});
    EXPECT_EQ(analysis.source_span, 200U);
    EXPECT_NEAR(static_cast<double>(analysis.resolution), 200.0 / 1022.0, 1e-12);

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    ASSERT_TRUE(reloaded.document.has_value());
    auto const reloaded_designed{
        fixture_find(reloaded.document->types(), "representations", "TemperatureQ10")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& quantized{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .linear_quantized
            .bit_width[reloaded.document->types().definitions().linear_quantized.index(quantized)],
        10U);
    EXPECT_EQ(reloaded.document->types().definitions().linear_quantized.reserved_codes
                  [reloaded.document->types().definitions().linear_quantized.index(quantized)],
              1U);
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .linear_quantized
            .clipping[reloaded.document->types().definitions().linear_quantized.index(quantized)],
        lispb::codegen::QuantizationClipping::clamp);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().linear_quantized.source
                              [reloaded.document->types().definitions().linear_quantized.index(
                                  quantized)])])),
              "Temperature");

    analysis = Analyzer::analyze_linear_quantized(reloaded.document->types(), *reloaded_designed);
    EXPECT_EQ(analysis.usable_code_count, (ExactCodeCount{.value = 1023, .two_to_64 = false}));
    EXPECT_EQ(analysis.source_minimum, lispb::codegen::PackedIntegerValue{-100});
    EXPECT_EQ(analysis.source_maximum, lispb::codegen::PackedIntegerValue{100});
    EXPECT_NEAR(static_cast<double>(analysis.maximum_rounding_error), 100.0 / 1022.0, 1e-12);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesIntegerVarintRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((integer-varint HealthVarint :source test::Health :encoding unsigned))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "representations", "HealthVarint")};
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
        fixture_find(reloaded.document->types(), "representations", "HealthVarint")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& varint{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_EQ(reloaded.document->types()
                  .definitions()
                  .integer_varints
                  .encoding[reloaded.document->types().definitions().integer_varints.index(varint)],
              lispb::codegen::IntegerVarintEncoding::unsigned_varint);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().integer_varints.source
                              [reloaded.document->types().definitions().integer_varints.index(
                                  varint)])])),
              "Health");

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
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((fixed-point VelocityQ8_8 :signed true :total-bits 16 :fractional-bits 8 :rounding nearest-even))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "representations", "VelocityQ8_8")};
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
        fixture_find(reloaded.document->types(), "representations", "VelocityQ8_8")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& fixed{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_TRUE(
        reloaded.document->types()
            .definitions()
            .fixed_points
            .signedness[reloaded.document->types().definitions().fixed_points.index(fixed)]);
    EXPECT_EQ(reloaded.document->types()
                  .definitions()
                  .fixed_points
                  .total_bits[reloaded.document->types().definitions().fixed_points.index(fixed)],
              16U);
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .fixed_points
            .fractional_bits[reloaded.document->types().definitions().fixed_points.index(fixed)],
        8U);
    EXPECT_EQ(reloaded.document->types()
                  .definitions()
                  .fixed_points
                  .rounding[reloaded.document->types().definitions().fixed_points.index(fixed)],
              lispb::codegen::FixedPointRounding::nearest_even);

    analysis = Analyzer::analyze_fixed_point(reloaded.document->types(), *reloaded_designed, 100);
    EXPECT_EQ(analysis.total_encoded_bits, 1'600U);
    EXPECT_DOUBLE_EQ(static_cast<double>(analysis.maximum_rounding_error), 1.0 / 512.0);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesMiniFloatRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((mini-float CompactFloat :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 15))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "representations", "CompactFloat")};
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
        fixture_find(reloaded.document->types(), "representations", "CompactFloat")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& mini_float{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .mini_floats
            .sign_bits[reloaded.document->types().definitions().mini_floats.index(mini_float)],
        1U);
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .mini_floats
            .exponent_bits[reloaded.document->types().definitions().mini_floats.index(mini_float)],
        5U);
    EXPECT_EQ(reloaded.document->types().definitions().mini_floats.significand_bits
                  [reloaded.document->types().definitions().mini_floats.index(mini_float)],
              10U);
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .mini_floats
            .exponent_bias[reloaded.document->types().definitions().mini_floats.index(mini_float)],
        15);

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
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((optional-sentinel OptionalHealth :source test::Health :sentinel Invalid))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{
        fixture_find(loaded.document->types(), "representations", "OptionalHealth")};
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
        fixture_find(reloaded.document->types(), "representations", "OptionalHealth")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& optional{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_EQ(
        reloaded.document->types().cpp_types().strings().view(
            reloaded.document->types().definitions().optional_sentinels.sentinel_name
                [reloaded.document->types().definitions().optional_sentinels.index(optional)]),
        "Invalid");
    EXPECT_EQ(reloaded.document->types().definitions().optional_sentinels.sentinel_value
                  [reloaded.document->types().definitions().optional_sentinels.index(optional)],
              lispb::codegen::PackedIntegerValue{1023});
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .optional_sentinels
            .bit_width[reloaded.document->types().definitions().optional_sentinels.index(optional)],
        10U);
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().optional_sentinels.source
                              [reloaded.document->types().definitions().optional_sentinels.index(
                                  optional)])])),
              "Health");

    analysis =
        Analyzer::analyze_optional_sentinel(reloaded.document->types(), *reloaded_designed, 1'000);
    EXPECT_EQ(analysis.total_encoded_bits, 10'000U);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesOptionalPresenceBitRepresentation) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("representations"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingHealthQ8")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(loaded.document->storage(),
                            R"((optional-presence-bit PresentHealth :source test::Health))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "representations", "PresentHealth")};
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
        fixture_find(reloaded.document->types(), "representations", "PresentHealth")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& optional{reloaded.document->types().definition(*reloaded_designed)};
    EXPECT_EQ(reloaded.document->types().definitions().optional_presence_bits.payload_bits
                  [reloaded.document->types().definitions().optional_presence_bits.index(optional)],
              10U);
    EXPECT_EQ(reloaded.document->types().definitions().optional_presence_bits.encoded_bits
                  [reloaded.document->types().definitions().optional_presence_bits.index(optional)],
              11U);
    EXPECT_EQ(
        reloaded.document->types().strings().view(reloaded.document->types().name(
            reloaded.document->types()
                .occurrences()
                .type[reloaded.document->types().occurrences().index(
                    reloaded.document->types().definitions().optional_presence_bits.source
                        [reloaded.document->types().definitions().optional_presence_bits.index(
                            optional)])])),
        "Health");

    analysis = Analyzer::analyze_optional_presence_bit(
        reloaded.document->types(), *reloaded_designed, 1'000);
    EXPECT_EQ(analysis.total_encoded_bits, 11'000U);
}

TEST(SchemaLoader, CreatesSavesReloadsAndAnalyzesSoa) {
    TemporarySchemaProject files;
    auto loaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(loaded.loaded) << diagnostic_text(loaded);
    ASSERT_TRUE(loaded.document.has_value());
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("soa"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingColumns")})};
    ASSERT_TRUE(existing.has_value());

    auto const schema{fixture_declaration(loaded.document->storage(), R"(
        (struct DesignedColumns (member states array @state)
            (member values array std::uint32_t (relation references ExistingColumns))))")};
    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        std::move(schema),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "soa", "DesignedColumns")};
    ASSERT_TRUE(designed.has_value());
    auto analysis{Analyzer::analyze_soa(
        loaded.document->types(), *designed, Variant{}, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.bytes_per_logical_element, 5U);
    EXPECT_EQ(analysis.total_payload_bytes, 500U);
    EXPECT_TRUE(analysis.diagnostics.empty());
    auto const& designed_soa{loaded.document->types().definition(*designed)};
    ASSERT_TRUE(lispb::schema::is_valid(
        loaded.document->types()
            .definitions()
            .soa_columns.relationship[loaded.document->types().definitions().soa_columns.index(
                loaded.document->types().definitions().soas.columns_of(designed_soa)[1])]));
    EXPECT_EQ(loaded.document->storage().strings.view(loaded.document->types().name(
                  loaded.document->types()
                      .occurrences()
                      .type[loaded.document->types().occurrences().index(
                          loaded.document->types()
                              .relationships()
                              .target[loaded.document->types().relationships().index(
                                  loaded.document->types().definitions().soa_columns.relationship
                                      [loaded.document->types().definitions().soa_columns.index(
                                          loaded.document->types().definitions().soas.columns_of(
                                              designed_soa)[1])])])])),
              "ExistingColumns");

    auto saved{loaded.document->save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_EQ(saved->size(), 1U);

    auto reloaded{load_lispb_schema(files.path("project.lispb"), "test-schema")};
    ASSERT_TRUE(reloaded.loaded) << diagnostic_text(reloaded);
    auto const reloaded_designed{
        fixture_find(reloaded.document->types(), "soa", "DesignedColumns")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& soa{reloaded.document->types().definition(*reloaded_designed)};
    ASSERT_EQ(reloaded.document->types().definitions().soas.columns_of(soa).size(), 2U);
    EXPECT_EQ(reloaded.document->storage().strings.view(
                  reloaded.document->types()
                      .definitions()
                      .soa_columns.name[reloaded.document->types().definitions().soa_columns.index(
                          reloaded.document->types().definitions().soas.columns_of(soa)[0])]),
              "states");
    EXPECT_EQ(reloaded.document->storage().strings.view(
                  reloaded.document->types()
                      .definitions()
                      .soa_columns.name[reloaded.document->types().definitions().soa_columns.index(
                          reloaded.document->types().definitions().soas.columns_of(soa)[1])]),
              "values");
    EXPECT_EQ(
        reloaded.document->types().strings().view(reloaded.document->types().name(
            reloaded.document->types()
                .occurrences()
                .type[reloaded.document->types().occurrences().index(
                    reloaded.document->types().definitions().soa_columns.semantic_type
                        [reloaded.document->types().definitions().soa_columns.index(
                            reloaded.document->types().definitions().soas.columns_of(soa)[0])])])),
        "State");
    ASSERT_TRUE(lispb::schema::is_valid(
        reloaded.document->types()
            .definitions()
            .soa_columns.relationship[reloaded.document->types().definitions().soa_columns.index(
                reloaded.document->types().definitions().soas.columns_of(soa)[1])]));
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types()
                              .relationships()
                              .target[reloaded.document->types().relationships().index(
                                  reloaded.document->types().definitions().soa_columns.relationship
                                      [reloaded.document->types().definitions().soa_columns.index(
                                          reloaded.document->types().definitions().soas.columns_of(
                                              soa)[1])])])])),
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
    auto const existing{loaded.document->find_declaration(
        {.origin = lispb::schema::TypeOrigin::declaration,
         .module_name = loaded.document->storage().strings.find("records"),
         .namespace_name = loaded.document->storage().strings.find("test"),
         .name = loaded.document->storage().strings.find("ExistingRecord")})};
    ASSERT_TRUE(existing.has_value());

    auto const created{loaded.document->allocate_declaration_id()};
    auto applied{loaded.document->apply(loaded.document->create_declaration(
        created,
        loaded.document->declarations()
            .module_index[loaded.document->declarations().index(*existing)],
        fixture_declaration(
            loaded.document->storage(),
            R"((record DesignedRecord (member state @state (relation references ExistingRecord)) (member values std::uint32_t :count 2)))"),
        std::nullopt))};
    ASSERT_TRUE(applied.has_value()) << applied.error().message;
    ASSERT_TRUE(*applied);

    auto const designed{fixture_find(loaded.document->types(), "records", "DesignedRecord")};
    auto const existing_type{fixture_find(loaded.document->types(), "records", "ExistingRecord")};
    ASSERT_TRUE(designed.has_value());
    ASSERT_TRUE(existing_type.has_value());
    auto const& designed_record{loaded.document->types().definition(*designed)};
    ASSERT_TRUE(lispb::schema::is_valid(
        loaded.document->types()
            .definitions()
            .record_members
            .relationship[loaded.document->types().definitions().record_members.index(
                loaded.document->types().definitions().records.members_of(designed_record)[0])]));
    EXPECT_EQ(
        loaded.document->types().occurrences().type[loaded.document->types().occurrences().index(
            loaded.document->types()
                .relationships()
                .target[loaded.document->types().relationships().index(
                    loaded.document->types().definitions().record_members.relationship
                        [loaded.document->types().definitions().record_members.index(
                            loaded.document->types().definitions().records.members_of(
                                designed_record)[0])])])],
        *existing_type);
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
        fixture_find(reloaded.document->types(), "records", "DesignedRecord")};
    ASSERT_TRUE(reloaded_designed.has_value());
    auto const& record{reloaded.document->types().definition(*reloaded_designed)};
    ASSERT_EQ(reloaded.document->types().definitions().records.members_of(record).size(), 2U);
    EXPECT_EQ(
        reloaded.document->storage().strings.view(
            reloaded.document->types()
                .definitions()
                .record_members.name[reloaded.document->types().definitions().record_members.index(
                    reloaded.document->types().definitions().records.members_of(record)[0])]),
        "state");
    EXPECT_EQ(reloaded.document->types().strings().view(reloaded.document->types().name(
                  reloaded.document->types()
                      .occurrences()
                      .type[reloaded.document->types().occurrences().index(
                          reloaded.document->types().definitions().record_members.semantic_type
                              [reloaded.document->types().definitions().record_members.index(
                                  reloaded.document->types().definitions().records.members_of(
                                      record)[0])])])),
              "State");
    ASSERT_TRUE(lispb::schema::is_valid(
        reloaded.document->types()
            .definitions()
            .record_members
            .relationship[reloaded.document->types().definitions().record_members.index(
                reloaded.document->types().definitions().records.members_of(record)[0])]));
    auto const reloaded_existing{
        fixture_find(reloaded.document->types(), "records", "ExistingRecord")};
    ASSERT_TRUE(reloaded_existing.has_value());
    EXPECT_EQ(reloaded.document->types()
                  .occurrences()
                  .type[reloaded.document->types().occurrences().index(
                      reloaded.document->types()
                          .relationships()
                          .target[reloaded.document->types().relationships().index(
                              reloaded.document->types().definitions().record_members.relationship
                                  [reloaded.document->types().definitions().record_members.index(
                                      reloaded.document->types().definitions().records.members_of(
                                          record)[0])])])],
              *reloaded_existing);
    EXPECT_EQ(
        reloaded.document->types()
            .definitions()
            .record_members.count[reloaded.document->types().definitions().record_members.index(
                reloaded.document->types().definitions().records.members_of(record)[1])],
        2);

    analysis = Analyzer::analyze_record(
        reloaded.document->types(), *reloaded_designed, AbiProfile::host_common());
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
