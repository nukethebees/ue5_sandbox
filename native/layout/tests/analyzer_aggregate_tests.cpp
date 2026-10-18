#include "analyzer_test_fixtures.hpp"

#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/type_definitions.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
namespace {

TEST(RecordAnalyzer, PointerUseHasPointerFactsRatherThanPointeeFacts) {
    lispb::CompilerStorage storage;
    auto const fixture{fixture_type(storage,
                                    R"((module records :header "Records.h"
            (record Record (member pointer (type-ref float :suffix "*")))))",
                                    "records",
                                    "Record")};
    auto const result{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};
    ASSERT_TRUE(result.members.front().element_facts.has_value());
    EXPECT_EQ(result.members.front().element_facts->size_bytes, sizeof(float*));
    EXPECT_EQ(result.size_bytes, sizeof(float*));
}

TEST(RecordAnalyzer, IntegerScalarAliasUsesRepresentationWithoutLosingIdentity) {
    lispb::CompilerStorage storage;
    auto const graph{fixture_graph(storage, R"(
        (module vitals :header "Vitals.h" :backend standard-library
            (integer-scalar Health :signed false :minimum 0 :maximum 15 :bit-width 4
                :cpp-emission alias :cpp-type std::uint16_t)
            (record Vessel (member health Health))
            (struct Fleet (member healths array Health))))")};
    auto const health{*fixture_find(graph, "vitals", "Health")};
    EXPECT_TRUE(
        ((graph.definition(health)).kind() == lispb::schema::TypeDefinitionKind::integer_scalar));
    EXPECT_EQ(graph.strings().view(graph.name(health)), "Health");
    EXPECT_EQ(physical_type_spelling(graph, health), "std::uint16_t");
    auto const scalar{Analyzer::analyze_integer_scalar(graph, health)};
    EXPECT_EQ(scalar.effective_bit_width, 4);
    auto const record{Analyzer::analyze_record(
        graph, *fixture_find(graph, "vitals", "Vessel"), AbiProfile::host_common())};
    EXPECT_EQ(record.size_bytes, 2);
    EXPECT_EQ(record.alignment_bytes, 2);
    auto const soa{Analyzer::analyze_soa(
        graph, *fixture_find(graph, "vitals", "Fleet"), {}, AbiProfile::host_common(), 10)};
    ASSERT_EQ(soa.columns.size(), 1);
    EXPECT_EQ(soa.columns[0].physical_type, "std::uint16_t");
    EXPECT_EQ(soa.columns[0].total_bytes, 20);
}

TEST(RecordAnalyzer, ScalarConstantsDoNotSupplyPhysicalLayout) {
    lispb::CompilerStorage storage;
    for (auto const emission : {lispb::codegen::IntegerScalarCppEmission::none,
                                lispb::codegen::IntegerScalarCppEmission::constants,
                                lispb::codegen::IntegerScalarCppEmission::constants_with_names}) {
        SCOPED_TRACE(lispb::codegen::integer_scalar_cpp_emission_name(emission));
        auto const representation{emission == lispb::codegen::IntegerScalarCppEmission::none
                                      ? std::string{}
                                      : std::string{" :cpp-type std::uint8_t"}};
        auto const graph{fixture_graph(
            storage,
            "(module reasons :header \"Reasons.h\" (integer-scalar Reason :signed false"
            " :minimum 0 :maximum 15 :bit-width 4 :cpp-emission " +
                std::string{lispb::codegen::integer_scalar_cpp_emission_name(emission)} +
                representation +
                " (code Unknown :value 0)) (record Consumer (member reason Reason)))")};
        auto const reason{*fixture_find(graph, "reasons", "Reason")};
        EXPECT_FALSE(physical_type_spelling(graph, reason).has_value());
        EXPECT_EQ(Analyzer::analyze_integer_scalar(graph, reason).effective_bit_width, 4);
        auto const layout{Analyzer::analyze_record(
            graph, *fixture_find(graph, "reasons", "Consumer"), AbiProfile::host_common())};
        EXPECT_FALSE(layout.size_bytes.has_value());
        EXPECT_FALSE(layout.diagnostics.empty());
    }
}

TEST(RecordAnalyzer, ReportsOffsetsInternalAndTailPadding) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[0].offset_bytes, 0);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[1].padding_before_bytes, 3);
    EXPECT_EQ(analysis.members[2].offset_bytes, 8);
    EXPECT_EQ(analysis.payload_bytes, 7);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 2);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_EQ(analysis.aggregate.element_count, 1);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 12);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 7);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 5);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 5);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto first_target{AbiProfile::host_common()};
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "synthetic wide target"});

    auto const first{Analyzer::analyze_record(fixture.types, fixture.type, first_target, 100)};
    auto const second{Analyzer::analyze_record(fixture.types, fixture.type, second_target, 100)};
    auto const comparison{Analyzer::compare_record_targets(first, second)};

    ASSERT_EQ(comparison.members.size(), 3U);
    EXPECT_EQ(comparison.first.size_bytes, 12);
    EXPECT_EQ(comparison.second.size_bytes, 24);
    ASSERT_TRUE(comparison.size_delta.has_value());
    EXPECT_EQ(comparison.size_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.size_delta->magnitude, 12);
    ASSERT_TRUE(comparison.alignment_delta.has_value());
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    ASSERT_TRUE(comparison.payload_delta.has_value());
    EXPECT_EQ(comparison.payload_delta->magnitude, 4);
    ASSERT_TRUE(comparison.internal_padding_delta.has_value());
    EXPECT_EQ(comparison.internal_padding_delta->magnitude, 4);
    ASSERT_TRUE(comparison.tail_padding_delta.has_value());
    EXPECT_EQ(comparison.tail_padding_delta->magnitude, 4);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.total_padding_delta.has_value());
    EXPECT_EQ(comparison.total_padding_delta->magnitude, 800);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 19);

    auto const& wide{comparison.members[1]};
    EXPECT_EQ(wide.name, "wide");
    EXPECT_EQ(wide.first.offset_bytes, 4);
    EXPECT_EQ(wide.second.offset_bytes, 8);
    ASSERT_TRUE(wide.element_size_delta.has_value());
    EXPECT_EQ(wide.element_size_delta->magnitude, 4);
    ASSERT_TRUE(wide.element_alignment_delta.has_value());
    EXPECT_EQ(wide.element_alignment_delta->magnitude, 4);
    ASSERT_TRUE(wide.offset_delta.has_value());
    EXPECT_EQ(wide.offset_delta->magnitude, 4);
    ASSERT_TRUE(wide.extent_delta.has_value());
    EXPECT_EQ(wide.extent_delta->magnitude, 4);
    ASSERT_TRUE(wide.padding_before_delta.has_value());
    EXPECT_EQ(wide.padding_before_delta->magnitude, 4);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_record_targets(first, first)};
    ASSERT_TRUE(identical.size_delta.has_value());
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.size_delta->magnitude, 0);
}

TEST(RecordAnalyzer, KeepsUnknownTargetFactsUnknownDuringComparison) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage, {record_member(storage, "value", "std::uint32_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const known{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto const comparison{Analyzer::compare_record_targets(known, unknown)};

    ASSERT_EQ(comparison.members.size(), 1U);
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.members[0].element_size_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target record:");
    }));
}

TEST(RecordAnalyzer, RejectsMismatchedTargetComparisonInputs) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "first", "std::uint32_t"),
                                record_member(storage, "second", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const first{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 11)};

    auto comparison{Analyzer::compare_record_targets(first, second)};
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.members[1].element_count = 2;
    comparison = Analyzer::compare_record_targets(first, second);
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible semantic identity"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_record_targets(first, second);
    EXPECT_TRUE(comparison.members.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different records"), std::string::npos);
}

TEST(UnionAnalyzer, ReportsMaximumExtentAlignmentAndAlternativeSlack) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage, "small", "std::uint8_t"),
                                    union_alternative(storage, "wide", "std::uint32_t"),
                                    union_alternative(storage, "bytes", "std::uint8_t", 6)});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};

    auto const analysis{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 100)};

    ASSERT_EQ(analysis.alternatives.size(), 3U);
    EXPECT_EQ(analysis.alternatives[0].extent_bytes, 1);
    EXPECT_EQ(analysis.alternatives[0].slack_bytes, 7);
    EXPECT_EQ(analysis.alternatives[0].total_slack_bytes, 700);
    EXPECT_EQ(analysis.alternatives[1].extent_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].slack_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].total_slack_bytes, 400);
    EXPECT_EQ(analysis.alternatives[2].extent_bytes, 6);
    EXPECT_EQ(analysis.alternatives[2].slack_bytes, 2);
    EXPECT_EQ(analysis.alternatives[2].total_slack_bytes, 200);
    EXPECT_EQ(analysis.largest_alternative_bytes, 6);
    EXPECT_EQ(analysis.tail_padding_bytes, 2);
    EXPECT_EQ(analysis.size_bytes, 8);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 200);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 13);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 8);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 512);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage, "small", "std::uint8_t"),
                                    union_alternative(storage, "wide", "std::uint32_t"),
                                    union_alternative(storage, "bytes", "std::uint8_t", 10)});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});

    auto const first{Analyzer::analyze_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second{Analyzer::analyze_union(fixture.types, fixture.type, second_profile, 100)};
    auto const comparison{Analyzer::compare_union_targets(first, second)};

    ASSERT_EQ(comparison.alternatives.size(), 3);
    EXPECT_EQ(comparison.first.size_bytes, 12);
    EXPECT_EQ(comparison.second.size_bytes, 16);
    EXPECT_EQ(comparison.largest_alternative_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.tail_padding_delta->magnitude, 4);
    EXPECT_EQ(comparison.size_delta->magnitude, 4);
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 400);
    EXPECT_EQ(comparison.total_tail_padding_delta->magnitude, 400);
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.complete_elements_per_cache_line_delta->magnitude, 1);
    EXPECT_EQ(comparison.cache_line_straddling_delta->magnitude, 12);

    auto const& wide{comparison.alternatives[1]};
    EXPECT_EQ(wide.name, "wide");
    EXPECT_EQ(wide.first.extent_bytes, 4);
    EXPECT_EQ(wide.second.extent_bytes, 8);
    EXPECT_EQ(wide.element_size_delta->magnitude, 4);
    EXPECT_EQ(wide.element_alignment_delta->magnitude, 4);
    EXPECT_EQ(wide.extent_delta->magnitude, 4);
    EXPECT_EQ(wide.slack_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(wide.total_slack_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_union_targets(first, first)};
    ASSERT_EQ(identical.alternatives.size(), 3);
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_storage_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(UnionAnalyzer, PreservesUnknownAndOverflowedTargetFactsDuringComparison) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage, {union_alternative(storage, "value", "std::uint32_t")});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    auto const known{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto comparison{Analyzer::compare_union_targets(known, unknown)};
    ASSERT_EQ(comparison.alternatives.size(), 1);
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.alternatives[0].element_size_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target raw union:");
    }));

    auto const overflow_first{Analyzer::analyze_union(fixture.types,
                                                      fixture.type,
                                                      AbiProfile::host_common(),
                                                      std::numeric_limits<std::uint64_t>::max())};
    auto const overflow_second{Analyzer::analyze_union(fixture.types,
                                                       fixture.type,
                                                       AbiProfile::host_common(),
                                                       std::numeric_limits<std::uint64_t>::max())};
    comparison = Analyzer::compare_union_targets(overflow_first, overflow_second);
    ASSERT_EQ(comparison.alternatives.size(), 1);
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(UnionAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialAlternatives) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage, "first", "std::uint32_t"),
                                    union_alternative(storage, "second", "std::uint16_t")});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    auto const first{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 11)};

    auto comparison{Analyzer::compare_union_targets(first, second)};
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.alternatives.back().element_count = 2;
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible semantic identity"),
              std::string::npos);

    second = first;
    second.alternatives.back().name = "renamed";
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different raw unions"),
              std::string::npos);
}

TEST(UnionAnalyzer, ReportsSelectedCountOverflowWithoutLosingPerObjectLayout) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage, {union_alternative(storage, "value", "std::uint64_t")});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};

    auto const analysis{Analyzer::analyze_union(fixture.types,
                                                fixture.type,
                                                AbiProfile::host_common(),
                                                std::numeric_limits<std::uint64_t>::max())};

    EXPECT_EQ(analysis.size_bytes, 8);
    EXPECT_EQ(analysis.alternatives[0].slack_bytes, 0);
    EXPECT_EQ(analysis.alternatives[0].total_slack_bytes, 0);
    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, HandlesNestedAggregatesAndUnknownOrOverflowedAlternatives) {
    lispb::CompilerStorage storage;
    auto types{fixture_graph(storage, R"(
        (module records :header "Records.h" :namespace records
            (record Record (member small std::uint8_t) (member wide std::uint32_t)))
        (module unions :header "Unions.h" :namespace unions
            (union Payload (alternative record records::Record)
                (alternative words std::uint16_t :count 6))))")};
    auto const payload{*fixture_find(types, "unions", "Payload")};
    auto analysis{Analyzer::analyze_union(types, payload, AbiProfile::host_common())};
    EXPECT_EQ(analysis.alternatives[0].element_facts->size_bytes, 8);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const unknown{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage, {union_alternative(storage, "value", "UnknownType")});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    analysis = Analyzer::analyze_union(unknown.types, unknown.type, AbiProfile::host_common());
    EXPECT_FALSE(analysis.size_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());

    auto const overflow{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage,
                                                      "values",
                                                      "std::uint64_t",
                                                      std::numeric_limits<std::uint64_t>::max())});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    analysis = Analyzer::analyze_union(overflow.types, overflow.type, AbiProfile::host_common());
    EXPECT_FALSE(analysis.size_bytes.has_value());
    EXPECT_FALSE(analysis.alternatives[0].extent_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(UnionAnalyzer, AnalyzesExplicitAlternativeDistribution) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage, "small", "std::uint8_t"),
                                    union_alternative(storage, "wide", "std::uint32_t"),
                                    union_alternative(storage, "bytes", "std::uint8_t", 6)});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    auto const layout{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    constexpr std::array workload{UnionDistributionEntry{.alternative_name = "small", .weight = 2},
                                  UnionDistributionEntry{.alternative_name = "wide", .weight = 1}};

    auto distribution{Analyzer::analyze_union_distribution(layout, workload, 10)};

    ASSERT_EQ(distribution.entries.size(), 2);
    EXPECT_EQ(distribution.valid_entry_count, 2);
    EXPECT_EQ(distribution.total_weight, 3);
    EXPECT_EQ(distribution.total_extent_bytes, 6);
    EXPECT_EQ(distribution.total_slack_bytes, 18);
    EXPECT_EQ(distribution.expected_extent_bytes_per_value, 2.0L);
    EXPECT_EQ(distribution.expected_slack_bytes_per_value, 6.0L);
    EXPECT_EQ(distribution.expected_selected_extent_bytes, 20.0L);
    EXPECT_EQ(distribution.expected_selected_slack_bytes, 60.0L);
    EXPECT_EQ(distribution.entries[0].weighted_extent_bytes, 2);
    EXPECT_EQ(distribution.entries[0].weighted_slack_bytes, 14);
    EXPECT_TRUE(distribution.diagnostics.empty());

    constexpr std::array invalid_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = 1},
        UnionDistributionEntry{.alternative_name = "small", .weight = 2},
        UnionDistributionEntry{.alternative_name = "missing", .weight = 1}};
    distribution = Analyzer::analyze_union_distribution(layout, invalid_workload, 10);
    EXPECT_EQ(distribution.valid_entry_count, 1);
    EXPECT_EQ(distribution.entries.size(), 1);
    EXPECT_EQ(distribution.diagnostics.size(), 2);

    constexpr std::array zero_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = 0}};
    distribution = Analyzer::analyze_union_distribution(layout, zero_workload, 10);
    EXPECT_EQ(distribution.total_weight, 0);
    EXPECT_FALSE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    auto const unknown_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};
    distribution = Analyzer::analyze_union_distribution(unknown_layout, workload, 10);
    ASSERT_EQ(distribution.entries.size(), 2);
    EXPECT_FALSE(distribution.total_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_slack_bytes.has_value());
    EXPECT_FALSE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflowing_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = maximum},
        UnionDistributionEntry{.alternative_name = "wide", .weight = maximum}};
    distribution = Analyzer::analyze_union_distribution(layout, overflowing_workload, maximum);
    EXPECT_FALSE(distribution.total_weight.has_value());
    EXPECT_FALSE(distribution.total_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_slack_bytes.has_value());
    EXPECT_FALSE(distribution.entries[1].weighted_extent_bytes.has_value());
    EXPECT_TRUE(distribution.expected_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());
}

TEST(UnionAnalyzer, ComparesExplicitAlternativeDistributionAcrossTargetProfiles) {
    lispb::CompilerStorage storage;
    auto const fixture{union_type(
        storage, {[&] {
            auto const new_unions{storage.declarations.unions.append()};
            storage.declarations.unions.name[storage.declarations.unions.index(new_unions)] =
                storage.strings.intern("Union");
            storage.declarations.unions
                .alternatives[storage.declarations.unions.index(new_unions)] =
                union_alternatives(storage,
                                   {union_alternative(storage, "small", "std::uint8_t"),
                                    union_alternative(storage, "wide", "std::uint32_t"),
                                    union_alternative(storage, "bytes", "std::uint8_t", 6)});
            storage.declarations.unions
                .export_specifier[storage.declarations.unions.index(new_unions)] = {};
            return new_unions;
        }()})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});
    auto const first_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, first_profile, 10)};
    auto const second_layout{
        Analyzer::analyze_union(fixture.types, fixture.type, second_profile, 10)};
    constexpr std::array workload{UnionDistributionEntry{.alternative_name = "small", .weight = 2},
                                  UnionDistributionEntry{.alternative_name = "wide", .weight = 1}};
    auto const first{Analyzer::analyze_union_distribution(first_layout, workload, 10)};
    auto const second{Analyzer::analyze_union_distribution(second_layout, workload, 10)};

    auto comparison{Analyzer::compare_union_distributions(first, second)};

    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_EQ(comparison.total_weight_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.total_extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_slack_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_slack_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_NEAR(*comparison.expected_extent_per_value_delta, 4.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_slack_per_value_delta, -4.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_selected_extent_delta, 40.0L / 3.0L, 1e-12L);
    EXPECT_NEAR(*comparison.expected_selected_slack_delta, -40.0L / 3.0L, 1e-12L);
    EXPECT_EQ(comparison.entries[1].extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[1].slack_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[1].slack_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_union_distributions(first, first)};
    ASSERT_EQ(identical.entries.size(), 2);
    EXPECT_EQ(identical.total_extent_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.expected_extent_per_value_delta, 0.0L);

    auto mismatched{second};
    mismatched.entries.back().weight = 2;
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible weight"), std::string::npos);

    mismatched = second;
    mismatched.selected_element_count = 11;
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different types or selected counts"),
              std::string::npos);

    mismatched = second;
    mismatched.entries.back().alternative_name = "renamed";
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives"),
              std::string::npos);

    mismatched = second;
    mismatched.type = lispb::schema::TypeId{second.type.value + 1};
    comparison = Analyzer::compare_union_distributions(first, mismatched);
    EXPECT_TRUE(comparison.entries.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflow_workload{
        UnionDistributionEntry{.alternative_name = "small", .weight = maximum},
        UnionDistributionEntry{.alternative_name = "wide", .weight = maximum}};
    auto const overflow{
        Analyzer::analyze_union_distribution(first_layout, overflow_workload, maximum)};
    comparison = Analyzer::compare_union_distributions(overflow, overflow);
    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_FALSE(comparison.total_weight_delta.has_value());
    EXPECT_FALSE(comparison.total_extent_delta.has_value());
    EXPECT_FALSE(comparison.entries[1].weighted_extent_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, LaysOutDiscriminantBeforeAlignedPayloadUnion) {
    lispb::CompilerStorage storage;
    auto const types{fixture_graph(storage, R"(
        (module events :header "Events.h" :namespace events
            (enum Kind std::uint8_t :count COUNT (value Small) (value Bytes) (value Spare)
                (value Invalid :sentinel true) (value COUNT)))
        (module payloads :header "Payloads.h" :namespace payloads
            (tagged-union Event :discriminant events::Kind
                (alternative small std::uint32_t :tag Small)
                (alternative bytes std::uint8_t :tag Bytes :count 6)))
        (module records :header "Records.h"
            (record Envelope (member event payloads::Event) (member suffix std::uint8_t))))")};
    auto const event{*fixture_find(types, "payloads", "Event")};
    auto const analysis{
        Analyzer::analyze_tagged_union(types, event, AbiProfile::host_common(), 100)};

    ASSERT_TRUE(analysis.discriminant_facts.has_value());
    EXPECT_EQ(analysis.discriminant_facts->size_bytes, 1);
    EXPECT_EQ(analysis.largest_alternative_bytes, 6);
    EXPECT_EQ(analysis.payload_size_bytes, 8);
    EXPECT_EQ(analysis.payload_alignment_bytes, 4);
    EXPECT_EQ(analysis.payload_offset_bytes, 4);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 0);
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    ASSERT_EQ(analysis.alternatives.size(), 2U);
    EXPECT_EQ(analysis.alternatives[0].payload_slack_bytes, 4);
    EXPECT_EQ(analysis.alternatives[1].payload_slack_bytes, 2);
    EXPECT_EQ(analysis.mapped_live_tags, (std::vector<std::string>{"Small", "Bytes"}));
    EXPECT_EQ(analysis.unmapped_live_tags, (std::vector<std::string>{"Spare"}));
    EXPECT_EQ(analysis.sentinel_tags, (std::vector<std::string>{"Invalid"}));
    EXPECT_EQ(analysis.count_sentinel_tag, "COUNT");
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 1'200);
    EXPECT_EQ(analysis.aggregate.total_discriminant_bytes, 100);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_internal_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 0);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 19);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 5);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 12);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 341);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
    EXPECT_EQ(analysis.alternatives[0].total_payload_slack_bytes, 400);
    EXPECT_EQ(analysis.alternatives[1].total_payload_slack_bytes, 200);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto const envelope{*fixture_find(types, "records", "Envelope")};
    auto const record{Analyzer::analyze_record(types, envelope, AbiProfile::host_common())};
    EXPECT_EQ(record.members[0].element_facts->size_bytes, 12);
    EXPECT_EQ(record.size_bytes, 16);
    EXPECT_TRUE(record.diagnostics.empty());

    AbiProfile unknown_memory{"unknown-memory"};
    unknown_memory.set("std::uint8_t",
                       {.size_bytes = 1,
                        .alignment_bytes = 1,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    unknown_memory.set("std::uint32_t",
                       {.size_bytes = 4,
                        .alignment_bytes = 4,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const unknown_memory_analysis{
        Analyzer::analyze_tagged_union(types, event, unknown_memory, 100)};
    EXPECT_EQ(unknown_memory_analysis.aggregate.total_storage_bytes, 1'200);
    EXPECT_FALSE(unknown_memory_analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(unknown_memory_analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(unknown_memory_analysis.diagnostics.empty());

    auto const overflow{Analyzer::analyze_tagged_union(
        types, event, AbiProfile::host_common(), std::numeric_limits<std::uint64_t>::max())};
    EXPECT_FALSE(overflow.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(overflow.aggregate.total_payload_bytes.has_value());
    EXPECT_FALSE(overflow.aggregate.total_internal_padding_bytes.has_value());
    EXPECT_FALSE(overflow.alternatives[0].total_payload_slack_bytes.has_value());
    EXPECT_FALSE(overflow.diagnostics.empty());

    constexpr std::array workload{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                  TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto distribution{Analyzer::analyze_tagged_union_distribution(analysis, workload, 100)};
    EXPECT_EQ(distribution.valid_entry_count, 2);
    EXPECT_EQ(distribution.total_weight, 4);
    EXPECT_EQ(distribution.total_payload_extent_bytes, 18);
    EXPECT_EQ(distribution.total_payload_slack_bytes, 14);
    EXPECT_EQ(distribution.expected_payload_extent_bytes_per_value, 4.5L);
    EXPECT_EQ(distribution.expected_payload_slack_bytes_per_value, 3.5L);
    EXPECT_EQ(distribution.expected_selected_payload_extent_bytes, 450.0L);
    EXPECT_EQ(distribution.expected_selected_payload_slack_bytes, 350.0L);
    ASSERT_EQ(distribution.entries.size(), 2U);
    EXPECT_EQ(distribution.entries[0].weighted_payload_extent_bytes, 12);
    EXPECT_EQ(distribution.entries[1].weighted_payload_slack_bytes, 2);
    EXPECT_TRUE(distribution.diagnostics.empty());

    constexpr std::array invalid_workload{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Small", .weight = 2},
        TaggedUnionDistributionEntry{.tag = "Spare", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Invalid", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "COUNT", .weight = 1},
        TaggedUnionDistributionEntry{.tag = "Missing", .weight = 1}};
    distribution = Analyzer::analyze_tagged_union_distribution(analysis, invalid_workload, 100);
    EXPECT_EQ(distribution.valid_entry_count, 1);
    EXPECT_EQ(distribution.entries.size(), 1U);
    EXPECT_GE(distribution.diagnostics.size(), 5U);

    constexpr std::array zero_workload{TaggedUnionDistributionEntry{.tag = "Small", .weight = 0}};
    distribution = Analyzer::analyze_tagged_union_distribution(analysis, zero_workload, 100);
    EXPECT_EQ(distribution.total_weight, 0);
    EXPECT_FALSE(distribution.expected_payload_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflowing_workload{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = maximum},
        TaggedUnionDistributionEntry{.tag = "Bytes", .weight = maximum}};
    distribution =
        Analyzer::analyze_tagged_union_distribution(analysis, overflowing_workload, maximum);
    EXPECT_FALSE(distribution.total_weight.has_value());
    EXPECT_FALSE(distribution.total_payload_extent_bytes.has_value());
    EXPECT_FALSE(distribution.total_payload_slack_bytes.has_value());
    EXPECT_FALSE(distribution.entries[0].weighted_payload_extent_bytes.has_value());
    EXPECT_TRUE(distribution.expected_payload_extent_bytes_per_value.has_value());
    EXPECT_FALSE(distribution.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    lispb::CompilerStorage storage;
    auto const fixture{tagged_union_type(storage)};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint8_t",
                       {.size_bytes = 2,
                        .alignment_bytes = 2,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = "synthetic wide target"});
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = "synthetic wide target"});
    auto const first{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, second_profile, 100)};

    auto const comparison{Analyzer::compare_tagged_union_targets(first, second)};

    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_EQ(comparison.discriminant_size_delta->magnitude, 1);
    EXPECT_EQ(comparison.discriminant_alignment_delta->magnitude, 1);
    EXPECT_EQ(comparison.largest_alternative_delta->magnitude, 10);
    EXPECT_EQ(comparison.payload_size_delta->magnitude, 12);
    EXPECT_EQ(comparison.payload_alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.payload_offset_delta->magnitude, 4);
    EXPECT_EQ(comparison.internal_padding_delta->magnitude, 3);
    EXPECT_EQ(comparison.tail_padding_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.size_delta->magnitude, 16);
    EXPECT_EQ(comparison.alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 1'600);
    EXPECT_EQ(comparison.total_discriminant_delta->magnitude, 100);
    EXPECT_EQ(comparison.total_payload_delta->magnitude, 1'200);
    EXPECT_EQ(comparison.total_internal_padding_delta->magnitude, 300);
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 25);
    EXPECT_EQ(comparison.complete_elements_per_cache_line_delta->magnitude, 2);
    EXPECT_EQ(comparison.first.mapped_live_tags, comparison.second.mapped_live_tags);
    EXPECT_EQ(comparison.first.unmapped_live_tags, comparison.second.unmapped_live_tags);
    EXPECT_EQ(comparison.first.sentinel_tags, comparison.second.sentinel_tags);

    auto const& small{comparison.alternatives[0]};
    EXPECT_EQ(small.first.tag, "Small");
    EXPECT_EQ(small.second.tag, "Small");
    EXPECT_EQ(small.element_size_delta->magnitude, 4);
    EXPECT_EQ(small.element_alignment_delta->magnitude, 4);
    EXPECT_EQ(small.extent_delta->magnitude, 4);
    EXPECT_EQ(small.payload_slack_delta->magnitude, 8);
    EXPECT_EQ(small.total_payload_slack_delta->magnitude, 800);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_tagged_union_targets(first, first)};
    ASSERT_EQ(identical.alternatives.size(), 2);
    EXPECT_EQ(identical.size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_storage_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(TaggedUnionAnalyzer, PreservesUnknownAndOverflowedTargetFactsDuringComparison) {
    lispb::CompilerStorage storage;
    auto const fixture{tagged_union_type(storage)};
    auto const known{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile{"unknown"}, 10)};

    auto comparison{Analyzer::compare_tagged_union_targets(known, unknown)};
    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_FALSE(comparison.discriminant_size_delta.has_value());
    EXPECT_FALSE(comparison.size_delta.has_value());
    EXPECT_FALSE(comparison.alternatives[0].element_size_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target tagged union:");
    }));

    auto const overflow_first{
        Analyzer::analyze_tagged_union(fixture.types,
                                       fixture.type,
                                       AbiProfile::host_common(),
                                       std::numeric_limits<std::uint64_t>::max())};
    comparison = Analyzer::compare_tagged_union_targets(overflow_first, overflow_first);
    ASSERT_EQ(comparison.alternatives.size(), 2);
    EXPECT_FALSE(comparison.total_storage_delta.has_value());
    EXPECT_FALSE(comparison.total_payload_delta.has_value());
    EXPECT_FALSE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(TaggedUnionAnalyzer, RejectsMismatchedTargetComparisonWithoutPartialAlternatives) {
    lispb::CompilerStorage storage;
    auto const fixture{tagged_union_type(storage)};
    auto const first{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto second{first};
    second.aggregate.element_count = 11;

    auto comparison{Analyzer::compare_tagged_union_targets(first, second)};
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.unmapped_live_tags.push_back("Other");
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("tag coverage semantics"),
              std::string::npos);

    second = first;
    second.alternatives.back().tag = "Small";
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique alternatives and tags"),
              std::string::npos);

    second = first;
    second.alternatives.back().element_count = 11;
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible tag"), std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_tagged_union_targets(first, second);
    EXPECT_TRUE(comparison.alternatives.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different tagged unions"),
              std::string::npos);
}

TEST(TaggedUnionAnalyzer, ComparesExplicitDistributionAcrossTargetProfiles) {
    lispb::CompilerStorage storage;
    auto const fixture{tagged_union_type(storage)};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint8_t",
                       {.size_bytes = 2,
                        .alignment_bytes = 2,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const first_layout{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, first_profile, 100)};
    auto const second_layout{
        Analyzer::analyze_tagged_union(fixture.types, fixture.type, second_profile, 100)};
    constexpr std::array distribution{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                      TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto const first{Analyzer::analyze_tagged_union_distribution(first_layout, distribution, 100)};
    auto const second{
        Analyzer::analyze_tagged_union_distribution(second_layout, distribution, 100)};

    auto const comparison{Analyzer::compare_tagged_union_distributions(first, second)};

    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_EQ(comparison.total_weight_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.total_payload_extent_delta->magnitude, 22);
    EXPECT_EQ(comparison.total_payload_slack_delta->magnitude, 26);
    EXPECT_EQ(comparison.expected_payload_extent_per_value_delta, 5.5L);
    EXPECT_EQ(comparison.expected_payload_slack_per_value_delta, 6.5L);
    EXPECT_EQ(comparison.expected_selected_payload_extent_delta, 550.0L);
    EXPECT_EQ(comparison.expected_selected_payload_slack_delta, 650.0L);
    EXPECT_EQ(comparison.entries[0].payload_extent_delta->magnitude, 4);
    EXPECT_EQ(comparison.entries[0].payload_slack_delta->magnitude, 8);
    EXPECT_EQ(comparison.entries[0].weighted_payload_extent_delta->magnitude, 12);
    EXPECT_EQ(comparison.entries[0].weighted_payload_slack_delta->magnitude, 24);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_tagged_union_distributions(first, first)};
    ASSERT_EQ(identical.entries.size(), 2);
    EXPECT_EQ(identical.total_payload_extent_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.expected_payload_extent_per_value_delta, 0.0L);
}

TEST(TaggedUnionAnalyzer, RejectsMismatchedOrOverflowedDistributionComparison) {
    lispb::CompilerStorage storage;
    auto const fixture{tagged_union_type(storage)};
    auto const layout{Analyzer::analyze_tagged_union(
        fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    constexpr std::array distribution{TaggedUnionDistributionEntry{.tag = "Small", .weight = 3},
                                      TaggedUnionDistributionEntry{.tag = "Bytes", .weight = 1}};
    auto const first{Analyzer::analyze_tagged_union_distribution(layout, distribution, 100)};
    auto second{first};
    second.entries.back().weight = 2;

    auto comparison{Analyzer::compare_tagged_union_distributions(first, second)};
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("incompatible alternative identity"),
              std::string::npos);

    second = first;
    second.selected_element_count = 101;
    comparison = Analyzer::compare_tagged_union_distributions(first, second);
    EXPECT_TRUE(comparison.entries.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different types or selected counts"),
              std::string::npos);

    constexpr auto maximum{std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array overflow_distribution{
        TaggedUnionDistributionEntry{.tag = "Small", .weight = maximum},
        TaggedUnionDistributionEntry{.tag = "Bytes", .weight = maximum}};
    auto const overflow{
        Analyzer::analyze_tagged_union_distribution(layout, overflow_distribution, maximum)};
    comparison = Analyzer::compare_tagged_union_distributions(overflow, overflow);
    ASSERT_EQ(comparison.entries.size(), 2);
    EXPECT_FALSE(comparison.total_weight_delta.has_value());
    EXPECT_FALSE(comparison.total_payload_extent_delta.has_value());
    EXPECT_FALSE(comparison.entries[0].weighted_payload_extent_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(RecordAnalyzer, HandlesFixedArraysAndNestedRecords) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage,
        {[&] {
             auto const new_records{storage.declarations.records.append()};
             storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                 storage.strings.intern("Inner");
             storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                 record_members(storage,
                                {record_member(storage, "tag", "std::uint8_t"),
                                 record_member(storage, "value", "std::uint32_t")});
             storage.declarations.records
                 .export_specifier[storage.declarations.records.index(new_records)] = {};
             return new_records;
         }(),
         [&] {
             auto const new_records{storage.declarations.records.append()};
             storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                 storage.strings.intern("Record");
             storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                 record_members(storage,
                                {record_member(storage, "prefix", "std::uint8_t"),
                                 record_member(storage, "inner", "Inner"),
                                 record_member(storage, "samples", "std::uint16_t", 2)});
             storage.declarations.records
                 .export_specifier[storage.declarations.records.index(new_records)] = {};
             return new_records;
         }()},
        "Record")};

    auto const analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common())};

    ASSERT_EQ(analysis.members.size(), 3U);
    EXPECT_EQ(analysis.members[1].element_facts->size_bytes, 8);
    EXPECT_EQ(analysis.members[1].offset_bytes, 4);
    EXPECT_EQ(analysis.members[2].element_count, 2);
    EXPECT_EQ(analysis.members[2].extent_bytes, 4);
    EXPECT_EQ(analysis.members[2].offset_bytes, 12);
    EXPECT_EQ(analysis.payload_bytes, 13);
    EXPECT_EQ(analysis.internal_padding_bytes, 3);
    EXPECT_EQ(analysis.tail_padding_bytes, 0);
    EXPECT_EQ(analysis.size_bytes, 16);
    EXPECT_EQ(analysis.alignment_bytes, 4);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, KeepsRelationshipIdsOutOfAbiLayout) {
    lispb::CompilerStorage storage;
    auto member{record_member(storage, "value", "std::uint32_t")};
    auto const related_member{storage.declarations.record_members.clone(member)};
    storage.declarations.record_members
        .relationship[storage.declarations.record_members.index(related_member)] = [&] {
        auto const new_semantic_relations{storage.declarations.semantic_relations.append()};
        storage.declarations.semantic_relations
            .kind[storage.declarations.semantic_relations.index(new_semantic_relations)] =
            lispb::codegen::SemanticRelationKind::references;
        storage.declarations.semantic_relations
            .target[storage.declarations.semantic_relations.index(new_semantic_relations)] =
            fixture_reference(storage, "Target");
        storage.declarations.semantic_relations
            .unit[storage.declarations.semantic_relations.index(new_semantic_relations)] =
            std::nullopt;
        return new_semantic_relations;
    }();
    auto records = [&](lispb::codegen::RecordMemberId value_member) {
        return std::vector{
            [&] {
                auto const new_records{storage.declarations.records.append()};
                storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                    storage.strings.intern("Target");
                storage.declarations.records
                    .members[storage.declarations.records.index(new_records)] =
                    record_members(storage, {record_member(storage, "id", "std::uint64_t")});
                storage.declarations.records
                    .export_specifier[storage.declarations.records.index(new_records)] = {};
                return new_records;
            }(),
            [&] {
                auto const new_records{storage.declarations.records.append()};
                storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                    storage.strings.intern("Record");
                storage.declarations.records
                    .members[storage.declarations.records.index(new_records)] =
                    record_members(storage, {std::move(value_member)});
                storage.declarations.records
                    .export_specifier[storage.declarations.records.index(new_records)] = {};
                return new_records;
            }()};
    };
    auto const plain{record_type(storage, records(std::move(member)))};
    auto const related{record_type(storage, records(std::move(related_member)))};

    auto const plain_analysis{
        Analyzer::analyze_record(plain.types, plain.type, AbiProfile::host_common(), 100)};
    auto const related_analysis{
        Analyzer::analyze_record(related.types, related.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(related_analysis.size_bytes, plain_analysis.size_bytes);
    EXPECT_EQ(related_analysis.alignment_bytes, plain_analysis.alignment_bytes);
    EXPECT_EQ(related_analysis.payload_bytes, plain_analysis.payload_bytes);
    EXPECT_EQ(related_analysis.internal_padding_bytes, plain_analysis.internal_padding_bytes);
    EXPECT_EQ(related_analysis.tail_padding_bytes, plain_analysis.tail_padding_bytes);
    EXPECT_EQ(related_analysis.members[0].offset_bytes, plain_analysis.members[0].offset_bytes);
    EXPECT_EQ(related_analysis.members[0].extent_bytes, plain_analysis.members[0].extent_bytes);
    EXPECT_EQ(related_analysis.aggregate.total_storage_bytes,
              plain_analysis.aggregate.total_storage_bytes);
    EXPECT_TRUE(related_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, KeepsUnknownAndOverflowedLayoutsUnknown) {
    lispb::CompilerStorage storage;
    auto const unknown{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage, {record_member(storage, "value", "UnknownType")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const unknown_analysis{
        Analyzer::analyze_record(unknown.types, unknown.type, AbiProfile::host_common())};
    EXPECT_FALSE(unknown_analysis.size_bytes.has_value());
    EXPECT_FALSE(unknown_analysis.diagnostics.empty());

    auto const overflow{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage,
                                              "values",
                                              "std::uint64_t",
                                              std::numeric_limits<std::uint64_t>::max())});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const overflow_analysis{
        Analyzer::analyze_record(overflow.types, overflow.type, AbiProfile::host_common())};
    EXPECT_FALSE(overflow_analysis.size_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.members[0].extent_bytes.has_value());
    EXPECT_FALSE(overflow_analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, ScalesAggregateWasteAndDiagnosesUnknownOrOverflowedTargetFacts) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};

    auto analysis{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.aggregate.element_count, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_EQ(analysis.aggregate.total_payload_bytes, 500);
    EXPECT_EQ(analysis.aggregate.total_internal_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.total_tail_padding_bytes, 0);
    EXPECT_EQ(analysis.aggregate.total_padding_bytes, 300);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 13);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    AbiProfile unknown_memory{"unknown-memory"};
    unknown_memory.set("std::uint8_t",
                       {.size_bytes = 1,
                        .alignment_bytes = 1,
                        .integer_signed = false,
                        .unsigned_value_bits = 8,
                        .provenance = {}});
    unknown_memory.set("std::uint32_t",
                       {.size_bytes = 4,
                        .alignment_bytes = 4,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    analysis = Analyzer::analyze_record(fixture.types, fixture.type, unknown_memory, 100);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 800);
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());

    analysis = Analyzer::analyze_record(fixture.types,
                                        fixture.type,
                                        AbiProfile::host_common(),
                                        std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_padding_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(RecordAnalyzer, CountsCacheLineAndPageStraddlingForAlignedContiguousArrays) {
    lispb::CompilerStorage storage;
    auto const periodic{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto analysis{
        Analyzer::analyze_record(periodic.types, periodic.type, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.size_bytes, 12);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 12);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);

    auto const larger_than_line{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage, {record_member(storage, "bytes", "std::uint8_t", 65)});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    analysis = Analyzer::analyze_record(
        larger_than_line.types, larger_than_line.type, AbiProfile::host_common(), 100);
    EXPECT_EQ(analysis.size_bytes, 65);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 100);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 1);
}

TEST(RecordAnalyzer, ReportsExplicitSequentialMemberAccessTraffic) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    auto access{Analyzer::analyze_record_member_access(record, "wide", AbiProfile::host_common())};

    EXPECT_EQ(access.element_count, 100);
    ASSERT_EQ(access.accesses.size(), 1);
    EXPECT_EQ(access.accesses.front().operation, AccessOperation::read);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_EQ(access.read_useful_bytes, 400);
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_EQ(access.logical_read_useful_bytes, 400);
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.object_footprint_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.cache_bytes_touched, 1'216);
    EXPECT_EQ(access.non_selected_cache_bytes, 816);
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l2.has_value());
    EXPECT_FALSE(access.cache_footprint_capacity.fits_l3.has_value());
    EXPECT_EQ(access.pages_touched, 1);
    EXPECT_EQ(access.page_bytes_touched, 4'096);
    EXPECT_EQ(access.non_selected_page_bytes, 3'696);
    EXPECT_TRUE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(record, "missing", AbiProfile::host_common());
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(
        record, "wide", AbiProfile::host_common(), AccessOperation::read_write, 3);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_EQ(access.logical_read_useful_bytes, 1'200);
    EXPECT_EQ(access.logical_write_useful_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.read_cache_lines_touched, 19);
    EXPECT_EQ(access.write_cache_lines_touched, 19);

    access = Analyzer::analyze_record_member_access(record,
                                                    "wide",
                                                    AbiProfile::host_common(),
                                                    AccessOperation::read,
                                                    std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_record_member_access(
        record, "wide", AbiProfile::host_common(), AccessOperation::read, 0);
    EXPECT_EQ(access.useful_bytes, 400);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_FALSE(access.logical_write_useful_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, UnionsMultipleSelectedMembersWithoutDoubleCounting) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 100)};
    std::vector<std::string> const members{"small", "medium", "small"};
    auto const access{Analyzer::analyze_record_access(record, members, AbiProfile::host_common())};

    EXPECT_EQ(access.member_names, (std::vector<std::string>{"small", "medium"}));
    EXPECT_EQ(access.useful_bytes, 300);
    EXPECT_EQ(access.object_footprint_bytes, 1'200);
    EXPECT_EQ(access.cache_lines_touched, 19);
    EXPECT_EQ(access.cache_bytes_touched, 1'216);
    EXPECT_EQ(access.non_selected_cache_bytes, 916);
    EXPECT_EQ(access.pages_touched, 1);
    EXPECT_TRUE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, ClassifiesIndividualMemberAccessOperations) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const abi{AbiProfile::host_common()};
    auto const record{Analyzer::analyze_record(fixture.types, fixture.type, abi, 100)};
    std::vector<AccessIntent> const accesses{
        {.name = "small", .operation = AccessOperation::read},
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_record_access(record, accesses, abi, 3)};

    EXPECT_EQ(analysis.accesses, accesses);
    EXPECT_EQ(analysis.useful_bytes, 700);
    EXPECT_EQ(analysis.read_useful_bytes, 300);
    EXPECT_EQ(analysis.write_useful_bytes, 600);
    EXPECT_EQ(analysis.logical_read_useful_bytes, 900);
    EXPECT_EQ(analysis.logical_write_useful_bytes, 1'800);
    EXPECT_EQ(analysis.cache_lines_touched, 19);
    EXPECT_EQ(analysis.read_cache_lines_touched, 19);
    EXPECT_EQ(analysis.read_cache_bytes_touched, 1'216);
    EXPECT_EQ(analysis.write_cache_lines_touched, 19);
    EXPECT_EQ(analysis.write_cache_bytes_touched, 1'216);
    EXPECT_EQ(analysis.read_pages_touched, 1);
    EXPECT_EQ(analysis.write_pages_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto conflicting{accesses};
    conflicting.push_back({.name = "small", .operation = AccessOperation::write});
    auto const normalized{Analyzer::analyze_record_access(record, conflicting, abi, 3)};
    EXPECT_EQ(normalized.accesses, accesses);
    EXPECT_EQ(normalized.read_useful_bytes, 300);
    EXPECT_EQ(normalized.write_useful_bytes, 600);
    EXPECT_FALSE(normalized.diagnostics.empty());
}

TEST(RecordAccessComparison, ReportsTargetLayoutAndRegionConsequencesForOneWorkload) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "small", "std::uint8_t"),
                                record_member(storage, "wide", "std::uint32_t"),
                                record_member(storage, "medium", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 2'000,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "Test wide target"});
    second_target.set_memory_facts({.cache_line_bytes = 128,
                                    .page_bytes = 8'192,
                                    .l1_data_cache_bytes = 2'000,
                                    .l2_cache_bytes = std::nullopt,
                                    .l3_cache_bytes = std::nullopt,
                                    .provenance = "Test second memory"});
    std::array const accesses{AccessIntent{.name = "small", .operation = AccessOperation::read},
                              AccessIntent{.name = "wide", .operation = AccessOperation::write}};
    auto const first_record{
        Analyzer::analyze_record(fixture.types, fixture.type, first_target, 100)};
    auto const second_record{
        Analyzer::analyze_record(fixture.types, fixture.type, second_target, 100)};
    auto const first{Analyzer::analyze_record_access(first_record, accesses, first_target, 3)};
    auto const second{Analyzer::analyze_record_access(second_record, accesses, second_target, 3)};

    auto const comparison{Analyzer::compare_record_access(first, second)};
    auto const identical{Analyzer::compare_record_access(first, first)};

    EXPECT_EQ(comparison.first.type, fixture.type);
    EXPECT_EQ(comparison.first.useful_bytes, 500);
    EXPECT_EQ(comparison.second.useful_bytes, 900);
    ASSERT_TRUE(comparison.useful_byte_delta.has_value());
    EXPECT_EQ(comparison.useful_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.read_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.write_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.write_useful_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.logical_write_useful_byte_delta.has_value());
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.object_footprint_byte_delta.has_value());
    EXPECT_EQ(comparison.object_footprint_byte_delta->magnitude, 1'200);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.cache_byte_delta.has_value());
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 1'216);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    ASSERT_TRUE(comparison.non_selected_page_byte_delta.has_value());
    EXPECT_EQ(comparison.non_selected_page_byte_delta->magnitude, 3'696);
    EXPECT_EQ(comparison.first.cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.cache_footprint_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());

    ASSERT_TRUE(identical.useful_byte_delta.has_value());
    EXPECT_EQ(identical.useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.cache_byte_delta.has_value());
    EXPECT_EQ(identical.cache_byte_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(RecordAccessComparison, PreservesUnknownAndOverflowedTargetFacts) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage, {record_member(storage, "value", "std::uint32_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    std::array const accesses{
        AccessIntent{.name = "value", .operation = AccessOperation::read_write}};
    auto const known_record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    auto const unknown_record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile{"Unknown target"}, 10)};
    auto const known{
        Analyzer::analyze_record_access(known_record, accesses, AbiProfile::host_common(), 2)};
    auto const unknown{
        Analyzer::analyze_record_access(unknown_record, accesses, AbiProfile{"Unknown target"}, 2)};

    auto const unknown_comparison{Analyzer::compare_record_access(known, unknown)};

    EXPECT_FALSE(unknown_comparison.useful_byte_delta.has_value());
    EXPECT_FALSE(unknown_comparison.object_footprint_byte_delta.has_value());
    EXPECT_FALSE(unknown_comparison.cache_byte_delta.has_value());
    EXPECT_TRUE(
        std::ranges::any_of(unknown_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target record access:");
        }));

    auto const overflow_record{Analyzer::analyze_record(fixture.types,
                                                        fixture.type,
                                                        AbiProfile::host_common(),
                                                        std::numeric_limits<std::uint64_t>::max())};
    auto const overflow{
        Analyzer::analyze_record_access(overflow_record, accesses, AbiProfile::host_common(), 2)};
    auto const overflow_comparison{Analyzer::compare_record_access(overflow, overflow)};

    EXPECT_FALSE(overflow_comparison.useful_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.object_footprint_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.cache_byte_delta.has_value());
    EXPECT_FALSE(overflow_comparison.diagnostics.empty());
}

TEST(RecordAccessComparison, RejectsMismatchedWorkloadsTransactionally) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "first", "std::uint32_t"),
                                record_member(storage, "second", "std::uint16_t")});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto const record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 10)};
    std::array const accesses{AccessIntent{.name = "first", .operation = AccessOperation::read},
                              AccessIntent{.name = "second", .operation = AccessOperation::write}};
    auto const first{
        Analyzer::analyze_record_access(record, accesses, AbiProfile::host_common(), 2)};
    auto second{first};
    second.accesses.front().operation = AccessOperation::write;

    auto comparison{Analyzer::compare_record_access(first, second)};

    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("access classifications"),
              std::string::npos);

    second = first;
    second.member_names.pop_back();
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("complete unique selected member set"),
              std::string::npos);

    second = first;
    second.multiplicity = 3;
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different access multiplicities"),
              std::string::npos);

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_record_access(first, second);
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different records"), std::string::npos);
}

TEST(RecordAnalyzer, MemberAccessHandlesSpanningMembersAndOverflow) {
    lispb::CompilerStorage storage;
    auto const fixture{record_type(
        storage, {[&] {
            auto const new_records{storage.declarations.records.append()};
            storage.declarations.records.name[storage.declarations.records.index(new_records)] =
                storage.strings.intern("Record");
            storage.declarations.records.members[storage.declarations.records.index(new_records)] =
                record_members(storage,
                               {record_member(storage, "prefix", "std::uint8_t"),
                                record_member(storage, "bytes", "std::uint8_t", 65)});
            storage.declarations.records
                .export_specifier[storage.declarations.records.index(new_records)] = {};
            return new_records;
        }()})};
    auto record{
        Analyzer::analyze_record(fixture.types, fixture.type, AbiProfile::host_common(), 2)};
    auto access{Analyzer::analyze_record_member_access(record, "bytes", AbiProfile::host_common())};
    EXPECT_EQ(record.size_bytes, 66);
    EXPECT_EQ(access.useful_bytes, 130);
    EXPECT_EQ(access.cache_lines_touched, 3);
    EXPECT_EQ(access.cache_bytes_touched, 192);
    EXPECT_EQ(access.non_selected_cache_bytes, 62);

    record = Analyzer::analyze_record(fixture.types,
                                      fixture.type,
                                      AbiProfile::host_common(),
                                      std::numeric_limits<std::uint64_t>::max());
    access = Analyzer::analyze_record_member_access(record, "bytes", AbiProfile::host_common());
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.cache_bytes_touched.has_value());
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(RecordAnalyzer, PeriodicMemberAccessMatchesBruteForceRegionUnion) {
    constexpr std::array counts{
        std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{7}, std::uint64_t{33}};
    for (std::uint64_t stride{1}; stride <= 32; ++stride) {
        for (std::uint64_t offset{}; offset < stride; ++offset) {
            for (std::uint64_t extent{1}; extent <= stride - offset; ++extent) {
                for (auto const count : counts) {
                    RecordAnalysis record{};
                    record.size_bytes = stride;
                    record.members.push_back(
                        RecordMemberAnalysis{.name = "selected",
                                             .semantic_type = {},
                                             .element_count = 1,
                                             .element_facts = std::nullopt,
                                             .offset_bytes = offset,
                                             .extent_bytes = extent,
                                             .padding_before_bytes = std::nullopt});
                    record.aggregate.element_count = count;
                    record.aggregate.total_storage_bytes = stride * count;

                    std::set<std::uint64_t> expected_lines;
                    for (std::uint64_t index{}; index < count; ++index) {
                        auto const begin{index * stride + offset};
                        auto const end{begin + extent - 1};
                        for (auto line{begin / 64}; line <= end / 64; ++line) {
                            expected_lines.insert(line);
                        }
                    }
                    auto const access{Analyzer::analyze_record_member_access(
                        record, "selected", AbiProfile::host_common())};
                    ASSERT_TRUE(access.cache_lines_touched.has_value());
                    EXPECT_EQ(*access.cache_lines_touched, expected_lines.size())
                        << "stride=" << stride << " offset=" << offset << " extent=" << extent
                        << " count=" << count;
                }
            }
        }
    }
}

TEST(RecordAnalyzer, PeriodicMultiMemberAccessMatchesBruteForceRegionUnion) {
    constexpr std::array counts{
        std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{7}, std::uint64_t{33}};
    std::vector<std::string> const selected_members{"first", "last"};
    for (std::uint64_t stride{2}; stride <= 32; ++stride) {
        for (auto const count : counts) {
            RecordAnalysis record{};
            record.size_bytes = stride;
            record.members.push_back(RecordMemberAnalysis{.name = "first",
                                                          .semantic_type = {},
                                                          .element_count = 1,
                                                          .element_facts = std::nullopt,
                                                          .offset_bytes = 0,
                                                          .extent_bytes = 1,
                                                          .padding_before_bytes = std::nullopt});
            record.members.push_back(RecordMemberAnalysis{.name = "last",
                                                          .semantic_type = {},
                                                          .element_count = 1,
                                                          .element_facts = std::nullopt,
                                                          .offset_bytes = stride - 1,
                                                          .extent_bytes = 1,
                                                          .padding_before_bytes = std::nullopt});
            record.aggregate.element_count = count;
            record.aggregate.total_storage_bytes = stride * count;

            std::set<std::uint64_t> expected_lines;
            for (std::uint64_t index{}; index < count; ++index) {
                expected_lines.insert(index * stride / 64);
                expected_lines.insert((index * stride + stride - 1) / 64);
            }
            auto const access{Analyzer::analyze_record_access(
                record, selected_members, AbiProfile::host_common())};
            ASSERT_TRUE(access.cache_lines_touched.has_value());
            EXPECT_EQ(*access.cache_lines_touched, expected_lines.size())
                << "stride=" << stride << " count=" << count;
        }
    }
}

} // namespace
} // namespace ioj::layout
