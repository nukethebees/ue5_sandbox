#include <ioj/layout/planner_session.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ioj::layout {
namespace {

auto session_manifest(bool const include_first = false) -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    if (include_first) {
        codegen::NormalModuleSchema first{};
        first.settings.name = "first";
        first.settings.header = "First.h";
        codegen::EnumSchema value{};
        value.name = "First";
        value.underlying_type =
            codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
        codegen::EnumeratorSchema enumerator{};
        enumerator.name = "Value";
        value.values.push_back(std::move(enumerator));
        first.declarations.push_back(std::move(value));
        manifest.modules.emplace_back(std::move(first));
    }

    codegen::NormalModuleSchema packed_module{};
    packed_module.settings.name = "packed";
    packed_module.settings.header = "Packed.h";
    codegen::PackedValueSchema packed{};
    packed.name = "Packet";
    packed.storage_type.name = "std::uint8_t";
    codegen::PackedFieldSchema bits{};
    bits.name = "bits";
    bits.type.name = "std::uint8_t";
    bits.bits = 8;
    packed.segments.emplace_back(std::move(bits));
    packed_module.declarations.push_back(std::move(packed));
    manifest.modules.emplace_back(std::move(packed_module));

    codegen::NormalModuleSchema records{};
    records.settings.name = "records";
    records.settings.header = "Records.h";
    codegen::RecordSchema record{};
    record.name = "Record";
    codegen::RecordMemberSchema member{};
    member.name = "value";
    member.type.name = "std::uint32_t";
    record.members.push_back(std::move(member));
    records.declarations.push_back(std::move(record));
    manifest.modules.emplace_back(std::move(records));
    return manifest;
}

auto distribution_manifest() -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema enums{};
    enums.settings.name = "kinds";
    enums.settings.header = "Kinds.h";
    codegen::EnumSchema kind{};
    kind.name = "Kind";
    kind.underlying_type =
        codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
    for (auto const* name : {"Small", "Large"}) {
        codegen::EnumeratorSchema value{};
        value.name = name;
        kind.values.push_back(std::move(value));
    }
    enums.declarations.push_back(std::move(kind));
    manifest.modules.emplace_back(std::move(enums));

    codegen::NormalModuleSchema unions{};
    unions.settings.name = "unions";
    unions.settings.header = "Unions.h";
    codegen::UnionSchema raw{};
    raw.name = "Raw";
    codegen::UnionAlternativeSchema small{};
    small.name = "small";
    small.type.name = "std::uint8_t";
    codegen::UnionAlternativeSchema large{};
    large.name = "large";
    large.type.name = "std::uint32_t";
    raw.alternatives = {std::move(small), std::move(large)};
    unions.declarations.push_back(std::move(raw));

    codegen::TaggedUnionSchema tagged{};
    tagged.name = "Tagged";
    tagged.discriminant.name = "Kind";
    codegen::TaggedUnionAlternativeSchema tagged_small{};
    tagged_small.name = "small";
    tagged_small.tag = "Small";
    tagged_small.type.name = "std::uint8_t";
    codegen::TaggedUnionAlternativeSchema tagged_large{};
    tagged_large.name = "large";
    tagged_large.tag = "Large";
    tagged_large.type.name = "std::uint32_t";
    tagged.alternatives = {std::move(tagged_small), std::move(tagged_large)};
    unions.declarations.push_back(std::move(tagged));
    manifest.modules.emplace_back(std::move(unions));
    return manifest;
}

auto varint_manifest(bool const include_second) -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::NormalModuleSchema scalars{};
    scalars.settings.name = "scalars";
    scalars.settings.header = "Scalars.h";
    codegen::IntegerScalarSchema value{};
    value.name = "Value";
    value.signedness = true;
    value.minimum_value = -100;
    value.maximum_value = 1000;
    scalars.declarations.push_back(std::move(value));
    manifest.modules.emplace_back(std::move(scalars));

    codegen::NormalModuleSchema representations{};
    representations.settings.name = "representations";
    representations.settings.header = "Representations.h";
    codegen::IntegerVarintSchema first{};
    first.name = "First";
    first.source.name = "Value";
    first.encoding = codegen::IntegerVarintEncoding::signed_varint;
    representations.declarations.push_back(std::move(first));
    if (include_second) {
        codegen::IntegerVarintSchema second{};
        second.name = "Second";
        second.source.name = "Value";
        second.encoding = codegen::IntegerVarintEncoding::zigzag_varint;
        representations.declarations.push_back(std::move(second));
    }
    manifest.modules.emplace_back(std::move(representations));
    return manifest;
}

TEST(PlannerSession, SelectionChangeDropsAccessAnalysisAndRepairsGraphIds) {
    auto document{lispb::schema::EditableSchemaDocument::from_manifest(session_manifest())};
    PlannerAnalysisSession session{document.types()};
    auto const record{*session.inputs.workspace.types().find_declared("records", "Record")};
    auto const packet{*session.inputs.workspace.types().find_declared("packed", "Packet")};
    auto const record_identity{session.inputs.workspace.types().type(record).identity};

    ASSERT_TRUE(session.inputs.selection.select_type(session.inputs.workspace.types(), record));
    session.inputs.selection.field = "value";
    session.refresh(&document);
    ASSERT_TRUE(session.results().record_access_analysis.has_value());
    ASSERT_TRUE(session.inputs.selection.select_type(session.inputs.workspace.types(), packet));
    session.refresh(&document);
    EXPECT_FALSE(session.results().record_access_analysis.has_value());
    EXPECT_FALSE(session.results().packed_access_analysis.has_value());
    EXPECT_TRUE(session.inputs.selection.field.empty());

    session.inputs.selection.select_type(session.inputs.workspace.types(), record);
    session.inputs.selection.field = "value";
    session.inputs.selection.record_access_members["value"] = AccessOperation::read;
    session.inputs.selection.record_access_set_explicit = true;
    auto reordered{lispb::schema::resolve_type_graph(session_manifest(true))};
    session.inputs.workspace.replace_types(reordered);
    session.inputs.selection.reconcile(session.inputs.workspace.types(), record_identity);
    session.refresh(nullptr);
    ASSERT_TRUE(session.inputs.selection.type.has_value());
    EXPECT_NE(*session.inputs.selection.type, record);
    EXPECT_EQ(session.inputs.selection.field, "value");
    EXPECT_TRUE(session.results().record_access_analysis.has_value());
}

TEST(PlannerSession, TargetAndVariantChangesRefreshComparisonsWithoutGuessingFacts) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(session_manifest())};
    auto const packet{*session.inputs.workspace.types().find_declared("packed", "Packet")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), packet);
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().packed_target_comparison.has_value());
    EXPECT_TRUE(session.results().packed_target_comparison->second.storage_facts.has_value());

    EXPECT_TRUE(session.set_comparison_abi(AbiProfile{"unknown"}));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().packed_target_comparison.has_value());
    EXPECT_FALSE(session.results().packed_target_comparison->second.storage_facts.has_value());
    EXPECT_EQ(session.status(packet), LayoutStatus::available);
    EXPECT_FALSE(session.set_comparison_abi(AbiProfile{"unknown"}));
    EXPECT_FALSE(session.refresh(nullptr));

    auto const variant{session.inputs.workspace.create_variant("alternative")};
    ASSERT_TRUE(session.inputs.workspace.set_packed_field_width(packet, "bits", 4));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().active_packed.has_value());
    EXPECT_EQ(session.results().active_packed->bits_used, 4);
    session.inputs.comparison_a_variant_id = LayoutWorkspace::baseline_variant_id;
    session.inputs.comparison_b_follows_active = false;
    session.inputs.comparison_b_variant_id = variant;
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().comparison_a_packed.has_value());
    ASSERT_TRUE(session.results().comparison_b_packed.has_value());
    EXPECT_EQ(session.results().comparison_a_packed->bits_used, 8);
    EXPECT_EQ(session.results().comparison_b_packed->bits_used, 4);
    session.inputs.comparison_a_variant_id = variant;
    ASSERT_TRUE(session.inputs.workspace.delete_variant(variant));
    session.refresh(nullptr);
    EXPECT_EQ(session.inputs.comparison_a_variant_id, LayoutWorkspace::baseline_variant_id);
    EXPECT_EQ(session.inputs.comparison_b_variant_id, LayoutWorkspace::baseline_variant_id);

    EXPECT_TRUE(session.set_primary_abi(AbiProfile{"unknown primary"}));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().active_packed.has_value());
    EXPECT_FALSE(session.results().active_packed->storage_facts.has_value());
    EXPECT_EQ(session.status(packet), LayoutStatus::unknown);
    EXPECT_FALSE(session.set_primary_abi(AbiProfile{"unknown primary"}));
    EXPECT_FALSE(session.refresh(nullptr));
}

TEST(PlannerSession, UnknownPrimaryTargetFactsRemainUnknown) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(session_manifest())};
    auto const record{*session.inputs.workspace.types().find_declared("records", "Record")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), record);
    EXPECT_TRUE(session.set_primary_abi(AbiProfile{"unknown"}));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.results().record_analysis->size_bytes.has_value());
    EXPECT_EQ(session.status(record), LayoutStatus::unknown);
}

TEST(PlannerSession, IrrelevantControlsDoNotInvalidateRecordAnalysis) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(session_manifest())};
    auto const record{*session.inputs.workspace.types().find_declared("records", "Record")};
    auto const variant{session.inputs.workspace.create_variant("compare")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), record);
    EXPECT_TRUE(session.refresh(nullptr));
    EXPECT_FALSE(session.refresh(nullptr));
    EXPECT_TRUE(session.set_varint_distribution(std::nullopt, {{.value = 1, .weight = 1}}, true));
    session.inputs.comparison_a_variant_id = variant;
    EXPECT_FALSE(session.refresh(nullptr));
    session.inputs.selection.field = "value";
    EXPECT_TRUE(session.refresh(nullptr));
    EXPECT_TRUE(session.results().record_access_analysis.has_value());
}

TEST(PlannerSession, UnionAndTaggedDistributionChangesRefreshAnalysis) {
    auto document{lispb::schema::EditableSchemaDocument::from_manifest(distribution_manifest())};
    PlannerAnalysisSession session{document.types()};
    auto const& types{session.inputs.workspace.types()};
    auto const raw{*types.find_declared("unions", "Raw")};
    auto const tagged{*types.find_declared("unions", "Tagged")};
    auto const raw_declaration{*document.find_declaration(types.type(raw).identity)};
    auto const tagged_declaration{*document.find_declaration(types.type(tagged).identity)};

    session.inputs.selection.select_type(types, raw);
    EXPECT_TRUE(session.set_union_distribution_weight(raw_declaration, "small", 1));
    session.refresh(&document);
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 1);
    EXPECT_TRUE(session.set_union_distribution_weight(raw_declaration, "small", 3));
    session.refresh(&document);
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 3);
    EXPECT_FALSE(session.set_union_distribution_weight(raw_declaration, "small", 3));
    EXPECT_FALSE(session.refresh(&document));

    session.inputs.selection.select_type(types, tagged);
    EXPECT_TRUE(session.set_tagged_union_distribution_weight(tagged_declaration, "Small", 2));
    session.refresh(&document);
    ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 2);
    EXPECT_TRUE(session.set_tagged_union_distribution_weight(tagged_declaration, "Small", 5));
    session.refresh(&document);
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 5);
    EXPECT_FALSE(session.set_tagged_union_distribution_weight(tagged_declaration, "Small", 5));
    EXPECT_FALSE(session.refresh(&document));
    EXPECT_TRUE(session.clear_tagged_union_distribution(tagged_declaration));
    EXPECT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_FALSE(session.clear_tagged_union_distribution(tagged_declaration));

    session.inputs.selection.select_type(types, raw);
    session.refresh(&document);
    EXPECT_TRUE(session.clear_union_distribution(raw_declaration));
    EXPECT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().union_distribution_analysis.has_value());
    EXPECT_FALSE(session.clear_union_distribution(raw_declaration));
}

TEST(PlannerSession, VarintDistributionsAndComparisonReferencesFollowGraphChanges) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(varint_manifest(true))};
    auto const& types{session.inputs.workspace.types()};
    auto const first{*types.find_declared("representations", "First")};
    auto const second{*types.find_declared("representations", "Second")};
    auto const first_identity{types.type(first).identity};
    session.inputs.selection.select_type(types, first);
    session.inputs.varint_comparison_type = types.type(second).identity;
    auto const source_identity{
        types
            .type(std::get<lispb::schema::IntegerVarintType>(types.type(first).definition)
                      .source.type)
            .identity};
    EXPECT_TRUE(
        session.set_varint_distribution(source_identity, {{.value = 5, .weight = 2}}, true));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().integer_varint_distribution.has_value());
    ASSERT_TRUE(session.results().integer_varint_distribution_comparison.has_value());
    EXPECT_EQ(session.results().integer_varint_distribution->total_weight, 2);

    EXPECT_TRUE(
        session.set_varint_distribution(source_identity, {{.value = 5, .weight = 7}}, true));
    session.refresh(nullptr);
    EXPECT_EQ(session.results().integer_varint_distribution->total_weight, 7);
    ASSERT_TRUE(session.results().integer_varint_distribution_comparison.has_value());
    EXPECT_EQ(session.results().integer_varint_distribution_comparison->second.total_weight, 7);
    EXPECT_FALSE(
        session.set_varint_distribution(source_identity, {{.value = 5, .weight = 7}}, true));
    EXPECT_FALSE(session.refresh(nullptr));

    session.inputs.workspace.replace_types(
        lispb::schema::resolve_type_graph(varint_manifest(false)));
    session.inputs.selection.reconcile(session.inputs.workspace.types(), first_identity);
    session.refresh(nullptr);
    EXPECT_FALSE(session.inputs.varint_comparison_type.has_value());
    EXPECT_FALSE(session.results().integer_varint_distribution_comparison.has_value());
}

TEST(PlannerSession, GraphEditsAndUndoRedoReconcileDistributionRows) {
    auto document{lispb::schema::EditableSchemaDocument::from_manifest(distribution_manifest())};
    PlannerAnalysisSession session{document.types()};
    auto const raw{*document.types().find_declared("unions", "Raw")};
    auto const tagged{*document.types().find_declared("unions", "Tagged")};
    auto const raw_identity{document.types().type(raw).identity};
    auto const raw_declaration{*document.find_declaration(raw_identity)};
    auto const tagged_declaration{
        *document.find_declaration(document.types().type(tagged).identity)};
    session.inputs.selection.select_type(session.inputs.workspace.types(), raw);
    EXPECT_TRUE(session.set_union_distribution_weight(raw_declaration, "small", 3));
    EXPECT_TRUE(session.set_tagged_union_distribution_weight(tagged_declaration, "Small", 2));

    auto renamed{*document.union_schema(raw_declaration)};
    renamed.alternatives.front().name = "renamed";
    ASSERT_TRUE(document
                    .apply(lispb::schema::ReplaceUnion{.declaration = raw_declaration,
                                                       .schema = std::move(renamed)})
                    .value());
    auto reduced{*document.tagged_union_schema(tagged_declaration)};
    reduced.alternatives.erase(reduced.alternatives.begin());
    ASSERT_TRUE(document
                    .apply(lispb::schema::ReplaceTaggedUnion{.declaration = tagged_declaration,
                                                             .schema = std::move(reduced)})
                    .value());
    session.replace_types(document, raw_identity);
    EXPECT_EQ(session.union_distributions().at(raw_declaration).at("renamed"), 3);
    EXPECT_FALSE(session.union_distributions().at(raw_declaration).contains("small"));
    EXPECT_FALSE(session.tagged_union_distributions().at(tagged_declaration).contains("Small"));

    ASSERT_TRUE(document.undo().value());
    session.replace_types(document, raw_identity);
    EXPECT_FALSE(session.tagged_union_distributions().at(tagged_declaration).contains("Small"));
    ASSERT_TRUE(document.redo().value());
    session.replace_types(document, raw_identity);
    EXPECT_FALSE(session.tagged_union_distributions().at(tagged_declaration).contains("Small"));
}

TEST(PlannerSession, DistributionWeightTransitionsRefreshCachedAnalysis) {
    auto document{lispb::schema::EditableSchemaDocument::from_manifest(distribution_manifest())};
    PlannerAnalysisSession session{document.types()};
    for (auto const tagged : {false, true}) {
        auto const& types{session.inputs.workspace.types()};
        auto const type{*types.find_declared("unions", tagged ? "Tagged" : "Raw")};
        auto const declaration{*document.find_declaration(types.type(type).identity)};
        auto const first{std::string{tagged ? "Small" : "small"}};
        auto const second{std::string{tagged ? "Large" : "large"}};
        auto set = [&](std::string name, std::uint64_t const weight) {
            return tagged
                     ? session.set_tagged_union_distribution_weight(
                           declaration, std::move(name), weight)
                     : session.set_union_distribution_weight(declaration, std::move(name), weight);
        };
        auto weights = [&]() -> PlannerAnalysisSession::DistributionWeights const* {
            return tagged ? session.tagged_union_distribution(declaration)
                          : session.union_distribution(declaration);
        };
        auto total = [&]() -> std::optional<std::uint64_t> {
            auto const& result{session.results()};
            if (tagged) {
                return result.tagged_union_distribution_analysis.has_value()
                         ? result.tagged_union_distribution_analysis->total_weight
                         : std::nullopt;
            }
            return result.union_distribution_analysis.has_value()
                     ? result.union_distribution_analysis->total_weight
                     : std::nullopt;
        };

        session.inputs.selection.select_type(types, type);
        session.refresh(&document);
        EXPECT_FALSE(set(first, 0));
        EXPECT_EQ(weights(), nullptr);
        EXPECT_FALSE(session.refresh(&document));
        EXPECT_TRUE(set(first, 1));
        EXPECT_TRUE(session.refresh(&document));
        EXPECT_EQ(total(), 1);
        EXPECT_FALSE(set(first, 1));
        EXPECT_FALSE(session.refresh(&document));
        EXPECT_TRUE(set(second, 2));
        EXPECT_TRUE(session.refresh(&document));
        EXPECT_EQ(total(), 3);
        EXPECT_FALSE(set("absent", 0));
        ASSERT_NE(weights(), nullptr);
        EXPECT_EQ(weights()->size(), 2U);
        EXPECT_FALSE(session.refresh(&document));
        EXPECT_TRUE(set(first, 0));
        ASSERT_NE(weights(), nullptr);
        EXPECT_FALSE(weights()->contains(first));
        EXPECT_EQ(weights()->at(second), 2);
        EXPECT_TRUE(session.refresh(&document));
        EXPECT_EQ(total(), 2);
        EXPECT_TRUE(set(second, 0));
        EXPECT_EQ(weights(), nullptr);
        EXPECT_TRUE(session.refresh(&document));
        EXPECT_FALSE(total().has_value());
        EXPECT_FALSE(set(second, 0));
        EXPECT_FALSE(session.refresh(&document));
    }
}

class TemporaryPlannerSchema {
  public:
    TemporaryPlannerSchema() {
        static int sequence{};
        directory_ = std::filesystem::temp_directory_path() /
                     ("planner-save-schema-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory_);
        std::ofstream types{directory_ / "types.lispb", std::ios::binary};
        std::ofstream output{directory_ / "modules.lispb", std::ios::binary};
        output << R"((module unions
  :header "Unions.h"
  (integer-scalar Before :signed false :minimum 0 :maximum 3)
  (enum Kind std::uint8_t
    (value Small :value "0")
    (value Large :value "1"))
  (union Earlier
    (alternative small std::uint8_t)
    (alternative large std::uint32_t))
  (union Later
    (alternative small std::uint8_t)
    (alternative large std::uint32_t))
  (tagged-union TaggedEarlier
    :discriminant Kind
    (alternative small std::uint8_t :tag Small)
    (alternative large std::uint32_t :tag Large))
  (tagged-union TaggedLater
    :discriminant Kind
    (alternative small std::uint8_t :tag Small)
    (alternative large std::uint32_t :tag Large)))
)";
        std::ofstream destination{directory_ / "destination.lispb", std::ios::binary};
        destination << R"((module destination
  :header "Destination.h"))";
    }
    ~TemporaryPlannerSchema() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    auto load() const -> lispb::schema::EditableSchemaDocument {
        auto const modules{std::array{directory_ / "modules.lispb"}};
        return lispb::schema::load_editable_schema_document(directory_ / "types.lispb", modules);
    }
    auto load_with_destination() const -> lispb::schema::EditableSchemaDocument {
        auto const modules{
            std::array{directory_ / "modules.lispb", directory_ / "destination.lispb"}};
        return lispb::schema::load_editable_schema_document(directory_ / "types.lispb", modules);
    }
    void append_external_edit() const {
        std::ofstream output{directory_ / "modules.lispb", std::ios::binary | std::ios::app};
        output << "\n; external edit\n";
    }
  private:
    std::filesystem::path directory_;
};

TEST(PlannerSession, SavePreservesDistributionOwnersAfterEarlierDeclarationsAreDeleted) {
    TemporaryPlannerSchema files;
    auto document{files.load()};
    auto const earlier{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "Earlier")).identity)};
    auto const later{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "Later")).identity)};
    auto const tagged_earlier{*document.find_declaration(
        document.types()
            .type(*document.types().find_declared("unions", "TaggedEarlier"))
            .identity)};
    auto const tagged_later{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "TaggedLater")).identity)};
    auto const later_identity{document.declaration(later)->identity};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(
        session.inputs.workspace.types(),
        *session.inputs.workspace.types().find_declared("unions", "Later"));
    session.set_union_distribution_weight(earlier, "small", 3);
    session.set_union_distribution_weight(later, "small", 7);
    session.set_tagged_union_distribution_weight(tagged_earlier, "Small", 5);
    session.set_tagged_union_distribution_weight(tagged_later, "Small", 11);

    ASSERT_TRUE(document.apply(lispb::schema::DeleteUnion{.declaration = earlier}).value());
    ASSERT_TRUE(
        document.apply(lispb::schema::DeleteTaggedUnion{.declaration = tagged_earlier}).value());
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    session.replace_types(document, later_identity);

    EXPECT_FALSE(session.union_distributions().contains(earlier));
    EXPECT_FALSE(session.tagged_union_distributions().contains(tagged_earlier));
    EXPECT_EQ(session.union_distributions().at(later).at("small"), 7);
    EXPECT_EQ(session.tagged_union_distributions().at(tagged_later).at("Small"), 11);
    EXPECT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 7);

    auto const& types{session.inputs.workspace.types()};
    session.inputs.selection.select_type(types, *types.find_declared("unions", "TaggedLater"));
    EXPECT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 11);
}

TEST(PlannerSession, SaveKeepsSurvivingUnionDistributionsWithOverlappingAlternativesDistinct) {
    TemporaryPlannerSchema files;
    auto document{files.load()};
    PlannerAnalysisSession session{document.types()};
    struct ExpectedDistribution {
        lispb::schema::TypeIdentity identity;
        lispb::schema::DeclarationId declaration;
        bool tagged;
        PlannerAnalysisSession::DistributionWeights weights;
    };
    std::vector<ExpectedDistribution> expected;
    for (auto const& name : {"Earlier", "Later", "TaggedEarlier", "TaggedLater"}) {
        auto const type{*document.types().find_declared("unions", name)};
        auto const identity{document.types().type(type).identity};
        auto const declaration{*document.find_declaration(identity)};
        auto const tagged{document.tagged_union_schema(declaration) != nullptr};
        auto const small_weight{static_cast<std::uint64_t>(expected.size() * 10 + 3)};
        auto const large_weight{small_weight + 4};
        auto const small{tagged ? "Small" : "small"};
        auto const large{tagged ? "Large" : "large"};
        if (tagged) {
            ASSERT_TRUE(
                session.set_tagged_union_distribution_weight(declaration, small, small_weight));
            ASSERT_TRUE(
                session.set_tagged_union_distribution_weight(declaration, large, large_weight));
        } else {
            ASSERT_TRUE(session.set_union_distribution_weight(declaration, small, small_weight));
            ASSERT_TRUE(session.set_union_distribution_weight(declaration, large, large_weight));
        }
        expected.push_back(
            {identity, declaration, tagged, {{small, small_weight}, {large, large_weight}}});
    }
    auto const before{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "Before")).identity)};
    ASSERT_TRUE(document.apply(lispb::schema::DeleteIntegerScalar{before}).value());
    session.replace_types(document, expected.front().identity);
    ASSERT_TRUE(session.refresh(&document));
    auto const saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    session.replace_types(document, expected.front().identity);

    auto const fresh{files.load()};
    EXPECT_EQ(*fresh.find_declaration(expected[1].identity), expected[0].declaration);
    EXPECT_EQ(*fresh.find_declaration(expected[3].identity), expected[2].declaration);
    EXPECT_EQ(session.union_distributions().size(), 2U);
    EXPECT_EQ(session.tagged_union_distributions().size(), 2U);
    for (auto const& owner : expected) {
        SCOPED_TRACE(owner.identity.name);
        auto const declaration{document.find_declaration(owner.identity)};
        ASSERT_TRUE(declaration.has_value());
        EXPECT_EQ(*declaration, owner.declaration);
        EXPECT_NE(*fresh.find_declaration(owner.identity), *declaration);
        auto const* weights{owner.tagged ? session.tagged_union_distribution(*declaration)
                                         : session.union_distribution(*declaration)};
        ASSERT_NE(weights, nullptr);
        EXPECT_EQ(*weights, owner.weights);
        session.inputs.selection.select_type(
            session.inputs.workspace.types(),
            *session.inputs.workspace.types().find(owner.identity));
        ASSERT_TRUE(session.refresh(&document));
        auto const total{owner.weights.begin()->second + owner.weights.rbegin()->second};
        if (owner.tagged) {
            ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
            EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, total);
        } else {
            ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
            EXPECT_EQ(session.results().union_distribution_analysis->total_weight, total);
        }
    }
}

TEST(PlannerSession, FailedSaveKeepsSelectionDistributionAndCachedResult) {
    TemporaryPlannerSchema files;
    auto document{files.load()};
    auto const type{*document.types().find_declared("unions", "Later")};
    auto const identity{document.types().type(type).identity};
    auto const declaration{*document.find_declaration(identity)};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), type);
    ASSERT_TRUE(session.set_union_distribution_weight(declaration, "small", 9));
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 9);

    ASSERT_TRUE(document
                    .apply(lispb::schema::RenameDeclaration{.declaration = declaration,
                                                            .new_name = "RenamedLater"})
                    .value());
    auto const revision{document.revision()};
    auto const preview{document.preview_source_updates().value()};
    files.append_external_edit();
    auto saved{document.save()};
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(document.revision(), revision);
    EXPECT_TRUE(document.can_undo());
    EXPECT_EQ(document.declaration(declaration)->identity.name, "RenamedLater");
    EXPECT_EQ(document.preview_source_updates().value()[0].updated, preview[0].updated);
    EXPECT_EQ(session.inputs.workspace.types().type(*session.inputs.selection.type).identity,
              identity);
    EXPECT_EQ(session.union_distributions().at(declaration).at("small"), 9);
    EXPECT_FALSE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 9);
}

TEST(PlannerSession, SavedMovePreservesSelectionReferenceAndDistribution) {
    TemporaryPlannerSchema files;
    auto document{files.load_with_destination()};
    auto const type{*document.types().find_declared("unions", "TaggedLater")};
    auto const declaration{*document.find_declaration(document.types().type(type).identity)};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), type);
    ASSERT_TRUE(session.set_tagged_union_distribution_weight(declaration, "Small", 13));
    auto moved{document.apply(lispb::schema::MoveDeclaration{
        .declaration = declaration, .module_index = 1, .insertion_index = std::nullopt})};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);
    auto const moved_identity{document.declaration(declaration)->identity};
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    session.replace_types(document, moved_identity);
    EXPECT_TRUE(session.refresh(&document));

    auto const& types{session.inputs.workspace.types()};
    ASSERT_TRUE(session.inputs.selection.type.has_value());
    EXPECT_EQ(types.type(*session.inputs.selection.type).identity, moved_identity);
    EXPECT_EQ(session.tagged_union_distributions().at(declaration).at("Small"), 13);
    ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 13);
    auto const& tagged{std::get<lispb::schema::TaggedUnionType>(
        types.type(*session.inputs.selection.type).definition)};
    EXPECT_EQ(types.type(tagged.discriminant.type).identity.name, "Kind");
}

TEST(PlannerSession, UnsafeRollbackKeepsDraftDistributionsAndCachedAnalysis) {
    TemporaryPlannerSchema files;
    auto document{files.load_with_destination()};
    auto const raw_type{*document.types().find_declared("unions", "Later")};
    auto const raw{*document.find_declaration(document.types().type(raw_type).identity)};
    auto const tagged{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "TaggedLater")).identity)};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), raw_type);
    session.set_union_distribution_weight(raw, "small", 9);
    session.set_tagged_union_distribution_weight(tagged, "Small", 13);
    ASSERT_TRUE(document
                    .apply(lispb::schema::MoveDeclaration{
                        .declaration = raw, .module_index = 1, .insertion_index = std::nullopt})
                    .value());
    auto const identity{document.declaration(raw)->identity};
    session.replace_types(document, identity);
    ASSERT_TRUE(session.refresh(&document));
    auto const revision{document.revision()};
    auto const workspace_revision{session.inputs.workspace.revision()};

    struct RestoreHook {
        lispb::schema::detail::SaveReplacementHook previous;
        ~RestoreHook() { lispb::schema::detail::set_save_replacement_hook_for_testing(previous); }
    };
    RestoreHook restore{lispb::schema::detail::set_save_replacement_hook_for_testing(
        [](auto const& source,
           auto const& destination,
           lispb::schema::detail::SaveReplaceFile real_replace) {
            if (source.filename() == "staged" && destination.filename() == "destination.lispb") {
                throw std::runtime_error{"Injected second publication failure"};
            }
            real_replace(source, destination);
            if (source.filename() == "staged") {
                std::ofstream output{destination, std::ios::binary | std::ios::app};
                output << "\n; concurrent edit\n";
            }
        })};
    auto const saved{document.save()};
    ASSERT_FALSE(saved.has_value());
    EXPECT_NE(saved.error().message.find("rollback skipped"), std::string::npos);
    EXPECT_EQ(document.revision(), revision);
    EXPECT_TRUE(document.can_undo());
    EXPECT_EQ(document.declaration(raw)->identity, identity);
    EXPECT_EQ(session.inputs.workspace.revision(), workspace_revision);
    EXPECT_EQ(session.inputs.selection.identity(), identity);
    EXPECT_EQ(session.union_distributions().at(raw).at("small"), 9);
    EXPECT_EQ(session.tagged_union_distributions().at(tagged).at("Small"), 13);
    EXPECT_FALSE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 9);
}

TEST(PlannerSession, ExplicitReloadSessionResetClearsDistributions) {
    TemporaryPlannerSchema files;
    auto document{files.load()};
    auto const raw_type{*document.types().find_declared("unions", "Later")};
    auto const identity{document.types().type(raw_type).identity};
    auto const raw{*document.find_declaration(identity)};
    auto const tagged{*document.find_declaration(
        document.types().type(*document.types().find_declared("unions", "TaggedLater")).identity)};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), raw_type);
    session.set_union_distribution_weight(raw, "small", 9);
    session.set_tagged_union_distribution_weight(tagged, "Small", 13);
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());

    auto inputs{session.inputs};
    document = files.load();
    session = PlannerAnalysisSession{document.types()};
    EXPECT_TRUE(session.union_distributions().empty());
    EXPECT_TRUE(session.tagged_union_distributions().empty());
    session.inputs = std::move(inputs);
    session.replace_types(document, identity);
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_EQ(session.inputs.selection.identity(), identity);
    EXPECT_FALSE(session.results().union_distribution_analysis.has_value());
    EXPECT_TRUE(session.union_distributions().empty());
    EXPECT_TRUE(session.tagged_union_distributions().empty());
    auto const& types{session.inputs.workspace.types()};
    session.inputs.selection.select_type(types, *types.find_declared("unions", "TaggedLater"));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().tagged_union_distribution_analysis.has_value());
}

} // namespace
} // namespace ioj::layout
