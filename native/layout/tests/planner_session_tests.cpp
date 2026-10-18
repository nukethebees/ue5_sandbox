#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/planner_session.hpp>

#include <codegen/schema/indices.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/editable_document.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/save_replace_file.h>
#include <lispb/schema/save_replacement_hook.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_identity.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

auto session_manifest(lispb::CompilerStorage& storage, bool const include_first = false)
    -> lispb::codegen::Manifest {
    std::string source;
    if (include_first) {
        source += R"((module first :header "First.h" (enum First std::uint8_t (value Value))))";
    }
    source += R"((module packed :header "Packed.h"
        (packed-value Packet :storage std::uint8_t (field bits std::uint8_t :bits 8)))
        (module records :header "Records.h" (record Record (member value std::uint32_t))))";
    return fixture_manifest(storage, source);
}

auto distribution_manifest(lispb::CompilerStorage& storage) -> lispb::codegen::Manifest {
    return fixture_manifest(storage, R"(
        (module kinds :header "Kinds.h" (enum Kind std::uint8_t (value Small) (value Large)))
        (module unions :header "Unions.h"
            (union Raw (alternative small std::uint8_t) (alternative large std::uint32_t))
            (tagged-union Tagged :discriminant Kind
                (alternative small std::uint8_t :tag Small)
                (alternative large std::uint32_t :tag Large))))");
}

auto varint_manifest(lispb::CompilerStorage& storage, bool const include_second)
    -> lispb::codegen::Manifest {
    std::string source{R"(
        (module scalars :header "Scalars.h"
            (integer-scalar Value :signed true :minimum -100 :maximum 1000))
        (module representations :header "Representations.h"
            (integer-varint First :source Value :encoding signed))"};
    if (include_second) {
        source += " (integer-varint Second :source Value :encoding zigzag)";
    }
    source += ')';
    return fixture_manifest(storage, source);
}

auto session_document() -> lispb::schema::EditableSchemaDocument {
    auto storage{std::make_unique<lispb::CompilerStorage>()};
    auto manifest{session_manifest(*storage)};
    return lispb::schema::EditableSchemaDocument::from_manifest(std::move(storage),
                                                                std::move(manifest));
}

auto distribution_document() -> lispb::schema::EditableSchemaDocument {
    auto storage{std::make_unique<lispb::CompilerStorage>()};
    auto manifest{distribution_manifest(*storage)};
    return lispb::schema::EditableSchemaDocument::from_manifest(std::move(storage),
                                                                std::move(manifest));
}

TEST(PlannerSession, SelectionChangeDropsAccessAnalysisAndRepairsGraphIds) {
    auto document{session_document()};
    PlannerAnalysisSession session{document.types()};
    auto const record{*fixture_find(session.inputs.workspace.types(), "records", "Record")};
    auto const packet{*fixture_find(session.inputs.workspace.types(), "packed", "Packet")};

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
    lispb::CompilerStorage storage;
    auto reordered_manifest{session_manifest(storage, true)};
    auto reordered{lispb::schema::resolve_type_graph(reordered_manifest)};
    session.inputs.workspace.replace_types(reordered);
    session.inputs.selection.reconcile(session.inputs.workspace.types(), std::nullopt);
    session.refresh(nullptr);
    ASSERT_TRUE(session.inputs.selection.type.has_value());
    EXPECT_NE(*session.inputs.selection.type, record);
    EXPECT_EQ(session.inputs.selection.field, "value");
    EXPECT_TRUE(session.results().record_access_analysis.has_value());
}

TEST(PlannerSession, TargetAndVariantChangesRefreshComparisonsWithoutGuessingFacts) {
    lispb::CompilerStorage storage;
    auto manifest{session_manifest(storage)};
    auto graph{lispb::schema::resolve_type_graph(manifest)};
    PlannerAnalysisSession session{graph};
    auto const packet{*fixture_find(session.inputs.workspace.types(), "packed", "Packet")};
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
    lispb::CompilerStorage storage;
    auto manifest{session_manifest(storage)};
    auto graph{lispb::schema::resolve_type_graph(manifest)};
    PlannerAnalysisSession session{graph};
    auto const record{*fixture_find(session.inputs.workspace.types(), "records", "Record")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), record);
    EXPECT_TRUE(session.set_primary_abi(AbiProfile{"unknown"}));
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().record_analysis.has_value());
    EXPECT_FALSE(session.results().record_analysis->size_bytes.has_value());
    EXPECT_EQ(session.status(record), LayoutStatus::unknown);
}

TEST(PlannerSession, IrrelevantControlsDoNotInvalidateRecordAnalysis) {
    lispb::CompilerStorage storage;
    auto manifest{session_manifest(storage)};
    auto graph{lispb::schema::resolve_type_graph(manifest)};
    PlannerAnalysisSession session{graph};
    auto const record{*fixture_find(session.inputs.workspace.types(), "records", "Record")};
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
    auto document{distribution_document()};
    PlannerAnalysisSession session{document.types()};
    auto const& types{session.inputs.workspace.types()};
    auto const raw{*fixture_find(types, "unions", "Raw")};
    auto const tagged{*fixture_find(types, "unions", "Tagged")};
    auto const raw_declaration{*document.find_declaration(types.identity(raw))};
    auto const tagged_declaration{*document.find_declaration(types.identity(tagged))};

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
    lispb::CompilerStorage storage;
    auto manifest{varint_manifest(storage, true)};
    auto graph{lispb::schema::resolve_type_graph(manifest)};
    PlannerAnalysisSession session{graph};
    auto const& types{session.inputs.workspace.types()};
    auto const first{*fixture_find(types, "representations", "First")};
    auto const second{*fixture_find(types, "representations", "Second")};
    auto const first_identity{types.identity(first)};
    session.inputs.selection.select_type(types, first);
    session.inputs.varint_comparison_type = types.identity(second);
    auto const source_identity{types.identity(types.occurrences().type[types.occurrences().index(
        types.definitions().integer_varints.source[types.definitions().integer_varints.index(
            types.definition(first))])])};
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

    auto replacement_manifest{varint_manifest(storage, false)};
    auto replacement_graph{lispb::schema::resolve_type_graph(replacement_manifest)};
    session.inputs.workspace.replace_types(replacement_graph);
    session.inputs.selection.reconcile(session.inputs.workspace.types(), first_identity);
    session.refresh(nullptr);
    EXPECT_FALSE(session.inputs.varint_comparison_type.has_value());
    EXPECT_FALSE(session.results().integer_varint_distribution_comparison.has_value());
}

TEST(PlannerSession, GraphEditsAndUndoRedoReconcileDistributionRows) {
    auto document{distribution_document()};
    PlannerAnalysisSession session{document.types()};
    auto const raw{*fixture_find(document.types(), "unions", "Raw")};
    auto const tagged{*fixture_find(document.types(), "unions", "Tagged")};
    auto const raw_identity{document.types().identity(raw)};
    auto const raw_declaration{*document.find_declaration(raw_identity)};
    auto const tagged_declaration{*document.find_declaration(document.types().identity(tagged))};
    session.inputs.selection.select_type(session.inputs.workspace.types(), raw);
    EXPECT_TRUE(session.set_union_distribution_weight(raw_declaration, "small", 3));
    EXPECT_TRUE(session.set_tagged_union_distribution_weight(tagged_declaration, "Small", 2));

    auto renamed{document.storage().declarations.unions.clone(*document.declaration_handle(
        raw_declaration, lispb::codegen::DeclarationKind::union_type))};
    auto& declarations{document.storage().declarations};
    document.storage()
        .declarations.unions.alternatives[document.storage().declarations.unions.index(renamed)] =
        [&] {
            auto const original_items{declarations.items(
                document.storage()
                    .declarations.unions
                    .alternatives[document.storage().declarations.unions.index(renamed)])};
            std::vector<lispb::codegen::UnionAlternativeId> rows{original_items.begin(),
                                                                 original_items.end()};
            rows.front() = [&] {
                auto value{declarations.union_alternatives.clone(rows.front())};
                declarations.union_alternatives.name[declarations.union_alternatives.index(value)] =
                    document.storage().strings.intern("renamed");
                return value;
            }();
            return declarations.range(rows);
        }();
    ASSERT_TRUE(document.apply(document.replace_declaration(raw_declaration, renamed)).value());
    auto reduced{document.storage().declarations.tagged_unions.clone(*document.declaration_handle(
        tagged_declaration, lispb::codegen::DeclarationKind::tagged_union))};
    document.storage()
        .declarations.tagged_unions
        .alternatives[document.storage().declarations.tagged_unions.index(reduced)] = [&] {
        auto const original_items{declarations.items(
            document.storage()
                .declarations.tagged_unions
                .alternatives[document.storage().declarations.tagged_unions.index(reduced)])};
        std::vector<lispb::codegen::TaggedUnionAlternativeId> rows{original_items.begin(),
                                                                   original_items.end()};
        rows.erase(rows.begin());
        return declarations.range(rows);
    }();
    ASSERT_TRUE(document.apply(document.replace_declaration(tagged_declaration, reduced)).value());
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
    auto document{distribution_document()};
    PlannerAnalysisSession session{document.types()};
    for (auto const tagged : {false, true}) {
        auto const& types{session.inputs.workspace.types()};
        auto const type{*fixture_find(types, "unions", tagged ? "Tagged" : "Raw")};
        auto const declaration{*document.find_declaration(types.identity(type))};
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
        document.types().identity(*fixture_find(document.types(), "unions", "Earlier")))};
    auto const later{*document.find_declaration(
        document.types().identity(*fixture_find(document.types(), "unions", "Later")))};
    auto const tagged_earlier{*document.find_declaration(
        document.types().identity(*fixture_find(document.types(), "unions", "TaggedEarlier")))};
    auto const tagged_later{*document.find_declaration(
        document.types().identity(*fixture_find(document.types(), "unions", "TaggedLater")))};
    auto const later_identity{
        document.declarations().identity[document.declarations().index(later)]};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(
        session.inputs.workspace.types(),
        *fixture_find(session.inputs.workspace.types(), "unions", "Later"));
    session.set_union_distribution_weight(earlier, "small", 3);
    session.set_union_distribution_weight(later, "small", 7);
    session.set_tagged_union_distribution_weight(tagged_earlier, "Small", 5);
    session.set_tagged_union_distribution_weight(tagged_later, "Small", 11);

    ASSERT_TRUE(document.apply(document.delete_declaration(earlier)).value());
    ASSERT_TRUE(document.apply(document.delete_declaration(tagged_earlier)).value());
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
    session.inputs.selection.select_type(types, *fixture_find(types, "unions", "TaggedLater"));
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
        auto const type{*fixture_find(document.types(), "unions", name)};
        auto const identity{document.types().identity(type)};
        auto const declaration{*document.find_declaration(identity)};
        auto const tagged{
            document.declaration_handle(declaration, lispb::codegen::DeclarationKind::tagged_union)
                .has_value()};
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
        document.types().identity(*fixture_find(document.types(), "unions", "Before")))};
    ASSERT_TRUE(document.apply(document.delete_declaration(before)).value());
    session.replace_types(document, expected.front().identity);
    ASSERT_TRUE(session.refresh(&document));
    auto const saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    session.replace_types(document, expected.front().identity);

    auto const fresh{files.load()};
    auto fresh_declaration = [&](lispb::schema::TypeIdentity identity) {
        for (auto* part : {&identity.module_name, &identity.namespace_name, &identity.name}) {
            *part = fresh.storage().strings.find(document.storage().strings.view(*part));
        }
        return fresh.find_declaration(identity);
    };
    auto const fresh_later{fresh_declaration(expected[1].identity)};
    auto const fresh_tagged_later{fresh_declaration(expected[3].identity)};
    ASSERT_TRUE(fresh_later.has_value());
    ASSERT_TRUE(fresh_tagged_later.has_value());
    EXPECT_EQ(*fresh_later, expected[0].declaration);
    EXPECT_EQ(*fresh_tagged_later, expected[2].declaration);
    EXPECT_EQ(session.union_distributions().size(), 2U);
    EXPECT_EQ(session.tagged_union_distributions().size(), 2U);
    for (auto const& owner : expected) {
        SCOPED_TRACE(document.storage().strings.view(owner.identity.name));
        auto const declaration{document.find_declaration(owner.identity)};
        ASSERT_TRUE(declaration.has_value());
        EXPECT_EQ(*declaration, owner.declaration);
        auto const fresh_id{fresh_declaration(owner.identity)};
        ASSERT_TRUE(fresh_id.has_value());
        EXPECT_NE(*fresh_id, *declaration);
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
    auto const type{*fixture_find(document.types(), "unions", "Later")};
    auto const identity{document.types().identity(type)};
    auto const declaration{*document.find_declaration(identity)};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), type);
    ASSERT_TRUE(session.set_union_distribution_weight(declaration, "small", 9));
    ASSERT_TRUE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 9);

    ASSERT_TRUE(document.apply(document.rename_declaration(declaration, "RenamedLater")).value());
    auto const revision{document.revision()};
    auto const preview{document.preview_source_updates().value()};
    files.append_external_edit();
    auto saved{document.save()};
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(document.revision(), revision);
    EXPECT_TRUE(document.can_undo());
    EXPECT_EQ(
        document.storage().strings.view(
            document.declarations().identity[document.declarations().index(declaration)].name),
        "RenamedLater");
    EXPECT_EQ(document.preview_source_updates().value()[0].updated, preview[0].updated);
    EXPECT_EQ(session.inputs.selection.identity(), identity);
    EXPECT_EQ(session.union_distributions().at(declaration).at("small"), 9);
    EXPECT_FALSE(session.refresh(&document));
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 9);
}

TEST(PlannerSession, SavedMovePreservesSelectionReferenceAndDistribution) {
    TemporaryPlannerSchema files;
    auto document{files.load_with_destination()};
    auto const type{*fixture_find(document.types(), "unions", "TaggedLater")};
    auto const declaration{*document.find_declaration(document.types().identity(type))};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), type);
    ASSERT_TRUE(session.set_tagged_union_distribution_weight(declaration, "Small", 13));
    auto moved{document.apply(document.move_declaration(declaration, 1, std::nullopt))};
    ASSERT_TRUE(moved.has_value()) << moved.error().message;
    ASSERT_TRUE(*moved);
    auto const moved_identity{
        document.declarations().identity[document.declarations().index(declaration)]};
    auto saved{document.save()};
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    session.replace_types(document, moved_identity);
    EXPECT_TRUE(session.refresh(&document));

    auto const& types{session.inputs.workspace.types()};
    ASSERT_TRUE(session.inputs.selection.type.has_value());
    EXPECT_EQ(types.identity(*session.inputs.selection.type), moved_identity);
    EXPECT_EQ(session.tagged_union_distributions().at(declaration).at("Small"), 13);
    ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 13);
    auto const& tagged{types.definition(*session.inputs.selection.type)};
    EXPECT_EQ(
        types.strings().view(types.name(types.occurrences().type[types.occurrences().index(
            types.definitions()
                .tagged_unions.discriminant[types.definitions().tagged_unions.index(tagged)])])),
        "Kind");
}

TEST(PlannerSession, UnsafeRollbackKeepsDraftDistributionsAndCachedAnalysis) {
    TemporaryPlannerSchema files;
    auto document{files.load_with_destination()};
    auto const raw_type{*fixture_find(document.types(), "unions", "Later")};
    auto const raw{*document.find_declaration(document.types().identity(raw_type))};
    auto const tagged{*document.find_declaration(
        document.types().identity(*fixture_find(document.types(), "unions", "TaggedLater")))};
    PlannerAnalysisSession session{document.types()};
    session.inputs.selection.select_type(session.inputs.workspace.types(), raw_type);
    session.set_union_distribution_weight(raw, "small", 9);
    session.set_tagged_union_distribution_weight(tagged, "Small", 13);
    ASSERT_TRUE(document.apply(document.move_declaration(raw, 1, std::nullopt)).value());
    auto const identity{document.declarations().identity[document.declarations().index(raw)]};
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
    EXPECT_EQ(document.declarations().identity[document.declarations().index(raw)], identity);
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
    auto const raw_type{*fixture_find(document.types(), "unions", "Later")};
    auto const identity{document.types().identity(raw_type)};
    auto const raw{*document.find_declaration(identity)};
    auto const tagged{*document.find_declaration(
        document.types().identity(*fixture_find(document.types(), "unions", "TaggedLater")))};
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
    session.inputs.selection.select_type(types, *fixture_find(types, "unions", "TaggedLater"));
    ASSERT_TRUE(session.refresh(&document));
    EXPECT_FALSE(session.results().tagged_union_distribution_analysis.has_value());
}

} // namespace
} // namespace ioj::layout
