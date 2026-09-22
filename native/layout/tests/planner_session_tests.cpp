#include <ioj/layout/planner_session.hpp>

#include <codegen/schema/schema_version.h>

#include <gtest/gtest.h>

namespace ioj::layout {
namespace {

auto session_manifest(bool const include_first = false) -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    if (include_first) {
        codegen::EnumModuleSchema first{};
        first.settings.name = "first";
        first.settings.header = "First.h";
        codegen::EnumSchema value{};
        value.name = "First";
        value.underlying_type =
            codegen::TypeRef{.name = "std::uint8_t", .suffix = {}, .nested = std::nullopt};
        codegen::EnumeratorSchema enumerator{};
        enumerator.name = "Value";
        value.values.push_back(std::move(enumerator));
        first.enums.push_back(std::move(value));
        manifest.modules.emplace_back(std::move(first));
    }

    codegen::PackedValueModuleSchema packed_module{};
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
    packed_module.values.push_back(std::move(packed));
    manifest.modules.emplace_back(std::move(packed_module));

    codegen::RecordModuleSchema records{};
    records.settings.name = "records";
    records.settings.header = "Records.h";
    codegen::RecordSchema record{};
    record.name = "Record";
    codegen::RecordMemberSchema member{};
    member.name = "value";
    member.type.name = "std::uint32_t";
    record.members.push_back(std::move(member));
    records.records.push_back(std::move(record));
    manifest.modules.emplace_back(std::move(records));
    return manifest;
}

auto distribution_manifest() -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::EnumModuleSchema enums{};
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
    enums.enums.push_back(std::move(kind));
    manifest.modules.emplace_back(std::move(enums));

    codegen::UnionModuleSchema unions{};
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
    unions.unions.push_back(std::move(raw));

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
    unions.tagged_unions.push_back(std::move(tagged));
    manifest.modules.emplace_back(std::move(unions));
    return manifest;
}

auto varint_manifest(bool const include_second) -> codegen::Manifest {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    codegen::ScalarModuleSchema scalars{};
    scalars.settings.name = "scalars";
    scalars.settings.header = "Scalars.h";
    codegen::IntegerScalarSchema value{};
    value.name = "Value";
    value.signedness = true;
    value.minimum_value = -100;
    value.maximum_value = 1000;
    scalars.scalars.push_back(std::move(value));
    manifest.modules.emplace_back(std::move(scalars));

    codegen::RepresentationModuleSchema representations{};
    representations.settings.name = "representations";
    representations.settings.header = "Representations.h";
    codegen::IntegerVarintSchema first{};
    first.name = "First";
    first.source.name = "Value";
    first.encoding = codegen::IntegerVarintEncoding::signed_varint;
    representations.integer_varints.push_back(std::move(first));
    if (include_second) {
        codegen::IntegerVarintSchema second{};
        second.name = "Second";
        second.source.name = "Value";
        second.encoding = codegen::IntegerVarintEncoding::zigzag_varint;
        representations.integer_varints.push_back(std::move(second));
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

TEST(PlannerSession, TargetAndVariantRevisionsRefreshComparisonsWithoutGuessingFacts) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(session_manifest())};
    auto const packet{*session.inputs.workspace.types().find_declared("packed", "Packet")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), packet);
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().packed_target_comparison.has_value());
    EXPECT_TRUE(session.results().packed_target_comparison->second.storage_facts.has_value());

    session.inputs.comparison_abi = AbiProfile{"unknown"};
    ++session.inputs.comparison_target_profile_revision;
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().packed_target_comparison.has_value());
    EXPECT_FALSE(session.results().packed_target_comparison->second.storage_facts.has_value());
    EXPECT_EQ(session.status(packet), LayoutStatus::available);

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

    session.inputs.abi = AbiProfile{"unknown primary"};
    ++session.inputs.target_profile_revision;
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().active_packed.has_value());
    EXPECT_FALSE(session.results().active_packed->storage_facts.has_value());
    EXPECT_EQ(session.status(packet), LayoutStatus::unknown);
}

TEST(PlannerSession, UnknownPrimaryTargetFactsRemainUnknown) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(session_manifest())};
    auto const record{*session.inputs.workspace.types().find_declared("records", "Record")};
    session.inputs.selection.select_type(session.inputs.workspace.types(), record);
    session.inputs.abi = AbiProfile{"unknown"};
    ++session.inputs.target_profile_revision;
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
    session.inputs.union_distribution_revision++;
    session.inputs.comparison_a_variant_id = variant;
    EXPECT_FALSE(session.refresh(nullptr));
    session.inputs.selection.field = "value";
    EXPECT_TRUE(session.refresh(nullptr));
    EXPECT_TRUE(session.results().record_access_analysis.has_value());
}

TEST(PlannerSession, UnionAndTaggedDistributionRevisionsRefreshAnalysis) {
    auto document{lispb::schema::EditableSchemaDocument::from_manifest(distribution_manifest())};
    PlannerAnalysisSession session{document.types()};
    auto const& types{session.inputs.workspace.types()};
    auto const raw{*types.find_declared("unions", "Raw")};
    auto const tagged{*types.find_declared("unions", "Tagged")};
    auto const raw_declaration{*document.find_declaration(types.type(raw).identity)};
    auto const tagged_declaration{*document.find_declaration(types.type(tagged).identity)};

    session.inputs.selection.select_type(types, raw);
    session.inputs.union_distributions[raw_declaration]["small"] = 1;
    ++session.inputs.union_distribution_revision;
    session.refresh(&document);
    ASSERT_TRUE(session.results().union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 1);
    session.inputs.union_distributions[raw_declaration]["small"] = 3;
    ++session.inputs.union_distribution_revision;
    session.refresh(&document);
    EXPECT_EQ(session.results().union_distribution_analysis->total_weight, 3);

    session.inputs.selection.select_type(types, tagged);
    session.inputs.tagged_union_distributions[tagged_declaration]["Small"] = 2;
    ++session.inputs.tagged_distribution_revision;
    session.refresh(&document);
    ASSERT_TRUE(session.results().tagged_union_distribution_analysis.has_value());
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 2);
    session.inputs.tagged_union_distributions[tagged_declaration]["Small"] = 5;
    ++session.inputs.tagged_distribution_revision;
    session.refresh(&document);
    EXPECT_EQ(session.results().tagged_union_distribution_analysis->total_weight, 5);
}

TEST(PlannerSession, VarintDistributionsAndComparisonReferencesFollowGraphChanges) {
    PlannerAnalysisSession session{lispb::schema::resolve_type_graph(varint_manifest(true))};
    auto const& types{session.inputs.workspace.types()};
    auto const first{*types.find_declared("representations", "First")};
    auto const second{*types.find_declared("representations", "Second")};
    auto const first_identity{types.type(first).identity};
    session.inputs.selection.select_type(types, first);
    session.inputs.varint_comparison_type = types.type(second).identity;
    session.inputs.varint_distribution_source =
        types
            .type(std::get<lispb::schema::IntegerVarintType>(types.type(first).definition)
                      .source.type)
            .identity;
    session.inputs.varint_distribution_entries.push_back({.value = 5, .weight = 2});
    session.inputs.varint_distribution_rows_present = true;
    ++session.inputs.varint_distribution_revision;
    session.refresh(nullptr);
    ASSERT_TRUE(session.results().integer_varint_distribution.has_value());
    ASSERT_TRUE(session.results().integer_varint_distribution_comparison.has_value());
    EXPECT_EQ(session.results().integer_varint_distribution->total_weight, 2);

    session.inputs.varint_distribution_entries.front().weight = 7;
    ++session.inputs.varint_distribution_revision;
    session.refresh(nullptr);
    EXPECT_EQ(session.results().integer_varint_distribution->total_weight, 7);

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
    session.inputs.union_distributions[raw_declaration]["small"] = 3;
    session.inputs.tagged_union_distributions[tagged_declaration]["Small"] = 2;

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
    EXPECT_EQ(session.inputs.union_distributions.at(raw_declaration).at("renamed"), 3);
    EXPECT_FALSE(session.inputs.union_distributions.at(raw_declaration).contains("small"));
    EXPECT_FALSE(
        session.inputs.tagged_union_distributions.at(tagged_declaration).contains("Small"));

    ASSERT_TRUE(document.undo().value());
    session.replace_types(document, raw_identity);
    EXPECT_FALSE(
        session.inputs.tagged_union_distributions.at(tagged_declaration).contains("Small"));
    ASSERT_TRUE(document.redo().value());
    session.replace_types(document, raw_identity);
    EXPECT_FALSE(
        session.inputs.tagged_union_distributions.at(tagged_declaration).contains("Small"));
}

} // namespace
} // namespace ioj::layout
