#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/access_operation.hpp>
#include <ioj/layout/analyzer/analyzer.hpp>
#include <ioj/layout/analyzer/record_access_analysis.hpp>
#include <ioj/layout/analyzer/record_analysis.hpp>
#include <ioj/layout/analyzer/record_target_comparison.hpp>
#include <ioj/layout/analyzer/tagged_union_analysis.hpp>
#include <ioj/layout/analyzer/union_analysis.hpp>
#include <ioj/layout/analyzer/union_distribution_analysis.hpp>
#include <ioj/layout/analyzer/union_distribution_entry.hpp>
#include <ioj/layout/editor_session.hpp>

namespace ioj::layout {
namespace analyzer_aggregate_tests {

class AggregateAnalysisTest : public testing::TestWithParam<AnalysisCase> {};

TEST_P(AggregateAnalysisTest, ReportsLayoutAndAccessFacts) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const document{editor.from_source(test.source)};
    ASSERT_TRUE(document.valid());
    test.verify(document);
}

AnalysisCase const cases[]{
    {"RecordPadding",
     R"((module game :header "Game.h"
        (record Value (member small std::uint8_t) (member wide std::uint32_t)
          (member tail std::uint8_t))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_record(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common(), 100)};
         EXPECT_EQ(result.payload_bytes, 6);
         EXPECT_EQ(result.internal_padding_bytes, 3);
         EXPECT_EQ(result.tail_padding_bytes, 3);
         EXPECT_EQ(result.size_bytes, 12);
         EXPECT_EQ(result.alignment_bytes, 4);
         EXPECT_EQ(result.members[1].offset_bytes, 4);
         EXPECT_EQ(result.members[2].offset_bytes, 8);
         EXPECT_EQ(result.aggregate.total_storage_bytes, 1200);
     }},
    {"NestedFixedArrays",
     R"((module game :header "Game.h"
        (record Pair (member x float) (member y float))
        (record Value (member pairs Pair :count 3) (member marker std::uint8_t))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_record(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_EQ(result.members[0].extent_bytes, 24);
         EXPECT_EQ(result.members[1].offset_bytes, 24);
         EXPECT_EQ(result.size_bytes, 28);
     }},
    {"PointerAndSemanticAlias",
     R"((module game :header "Game.h"
        (record Pair (member x float) (member y float))
        (integer-scalar Count :signed false :minimum 0 :maximum 100
          :cpp-emission alias :cpp-type std::uint16_t)
        (record Value (member pointer (type-ref Pair :suffix "*")) (member count Count))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_record(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_EQ(result.members[0].extent_bytes, sizeof(void*));
         EXPECT_EQ(result.members[1].extent_bytes, 2);
     }},
    {"UnknownMemberPreservesPrefix",
     R"((module game :header "Game.h"
        (record Value (member first std::uint32_t) (member unknown Opaque)
          (member last double))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_record(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_EQ(result.members[0].offset_bytes, 0);
         EXPECT_FALSE(result.members[2].offset_bytes);
         EXPECT_EQ(result.members[2].extent_bytes, 8);
         EXPECT_FALSE(result.size_bytes);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"AggregateOverflow",
     R"((module game :header "Game.h"
        (record Value (member items std::uint64_t :count 18446744073709551615))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_record(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_FALSE(result.members[0].extent_bytes);
         EXPECT_FALSE(result.size_bytes);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"RecordTargetsAndWorkload",
     R"((module game :header "Game.h"
        (record Value (member small std::uint8_t) (member wide std::uint32_t))))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         auto const abi{AbiProfile::host_common()};
         auto wide{abi};
         wide.set("std::uint32_t",
                  {.size_bytes = 8,
                   .alignment_bytes = 8,
                   .integer_signed = false,
                   .unsigned_value_bits = 32,
                   .provenance = {}});
         auto const first{Analyzer::analyze_record(doc.types(), type, abi, 100)};
         auto const second{Analyzer::analyze_record(doc.types(), type, wide, 100)};
         auto const comparison{Analyzer::compare_record_targets(first, second)};
         EXPECT_EQ(comparison.size_delta->magnitude, 8);
         EXPECT_EQ(comparison.total_storage_delta->magnitude, 800);
         auto const access{
             Analyzer::analyze_record_member_access(first, "wide", abi, AccessOperation::read, 3)};
         EXPECT_EQ(access.useful_bytes, 400);
         EXPECT_EQ(access.logical_read_useful_bytes, 1200);
         EXPECT_FALSE(
             Analyzer::analyze_record_member_access(first, "missing", abi).diagnostics.empty());
     }},
    {"UnionSlackAndDistribution",
     R"((module game :header "Game.h"
        (union Value (alternative word std::uint32_t)
          (alternative bytes std::uint8_t :count 10))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_union(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common(), 100)};
         EXPECT_EQ(result.largest_alternative_bytes, 10);
         EXPECT_EQ(result.size_bytes, 12);
         EXPECT_EQ(result.tail_padding_bytes, 2);
         EXPECT_EQ(result.alternatives[0].slack_bytes, 8);
         EXPECT_EQ(result.alternatives[1].slack_bytes, 2);
         std::array const entries{UnionDistributionEntry{"word", 3},
                                  UnionDistributionEntry{"bytes", 1}};
         auto const distribution{Analyzer::analyze_union_distribution(result, entries, 100)};
         EXPECT_EQ(distribution.expected_extent_bytes_per_value, 5.5L);
         EXPECT_EQ(distribution.expected_slack_bytes_per_value, 6.5L);
         EXPECT_EQ(distribution.expected_selected_extent_bytes, 550.0L);
         EXPECT_TRUE(distribution.diagnostics.empty());
     }},
    {"NestedUnion",
     R"((module game :header "Game.h"
        (record Pair (member x double) (member y double))
        (union Value (alternative pair Pair) (alternative byte std::uint8_t))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_union(doc.types(),
                                                   doc.find_type("game", "Value").value(),
                                                   AbiProfile::host_common(),
                                                   std::numeric_limits<std::uint64_t>::max())};
         EXPECT_EQ(result.size_bytes, 16);
         EXPECT_EQ(result.alignment_bytes, 8);
         EXPECT_FALSE(result.aggregate.total_storage_bytes);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"TaggedUnion",
     R"((module game :header "Game.h"
        (enum Kind std::uint8_t :count Count
          (value Small) (value Bytes) (value Spare) (value Invalid :sentinel true) (value Count))
        (tagged-union Value :discriminant Kind
          (alternative small std::uint32_t :tag Small)
          (alternative bytes std::uint8_t :tag Bytes :count 10))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_tagged_union(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common(), 100)};
         EXPECT_EQ(result.payload_offset_bytes, 4);
         EXPECT_EQ(result.payload_size_bytes, 12);
         EXPECT_EQ(result.internal_padding_bytes, 3);
         EXPECT_EQ(result.size_bytes, 16);
         EXPECT_EQ(result.aggregate.total_storage_bytes, 1600);
         EXPECT_EQ(result.mapped_live_tags.size(), 2);
         EXPECT_EQ(result.unmapped_live_tags, std::vector<std::string>{"Spare"});
         EXPECT_EQ(result.sentinel_tags, std::vector<std::string>{"Invalid"});
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         AggregateAnalysisTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

}
}
