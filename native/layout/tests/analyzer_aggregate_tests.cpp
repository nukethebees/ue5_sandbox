#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/editor_session.hpp>

namespace ioj::layout {
namespace analyzer_aggregate_tests {

class AggregateAnalysisTest : public AnalysisTest {};

TEST_P(AggregateAnalysisTest, ReportsLayoutAndAccessFacts) {
    verify_case();
}

AnalysisCase const cases[]{
    {"RecordPadding",
     R"((module game :header "Game.h"
        (record Value (member small std::uint8_t) (member wide std::uint32_t)
          (member tail std::uint8_t))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 100})};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         auto const& members{context.storage.components};
         EXPECT_EQ(result,
                   (AggregateLayout{.members = {result.members.begin, 3},
                                    .size_bytes = 12,
                                    .alignment_bytes = 4,
                                    .payload_bytes = 6,
                                    .internal_padding_bytes = 3,
                                    .tail_padding_bytes = 3}));
         EXPECT_EQ(members.offset_bytes[result.members.begin + 1], 4);
         EXPECT_EQ(members.offset_bytes[result.members.begin + 2], 8);
         EXPECT_EQ(context.storage.totals[0].total_storage_bytes, 1200);
     }},
    {"NestedFixedArrays",
     R"((module game :header "Game.h"
        (record Pair (member x float) (member y float))
        (record Value (member pairs Pair :count 3) (member marker std::uint8_t))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_EQ(context.storage.components.extent_bytes[result.members.begin], 24);
         EXPECT_EQ(context.storage.components.offset_bytes[result.members.begin + 1], 24);
         EXPECT_EQ(result.size_bytes, 28);
     }},
    {"PointerAndSemanticAlias",
     R"((module game :header "Game.h"
        (record Pair (member x float) (member y float))
        (integer-scalar Count :signed false :minimum 0 :maximum 100
          :cpp-emission alias :cpp-type std::uint16_t)
        (record Value (member pointer (type-ref Pair :suffix "*")) (member count Count))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const members{context.storage.types.aggregate[run.analysis.value]->members};
         EXPECT_EQ(context.storage.components.extent_bytes[members.begin], sizeof(void*));
         EXPECT_EQ(context.storage.components.extent_bytes[members.begin + 1], 2);
     }},
    {"UnknownMemberPreservesPrefix",
     R"((module game :header "Game.h"
        (record Value (member first std::uint32_t) (member unknown Opaque)
          (member last double))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {}, true)};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_EQ(context.storage.components.offset_bytes[result.members.begin], 0);
         EXPECT_FALSE(context.storage.components.offset_bytes[result.members.begin + 2]);
         EXPECT_EQ(context.storage.components.extent_bytes[result.members.begin + 2], 8);
         EXPECT_EQ(result, (AggregateLayout{.members = {result.members.begin, 3}}));
     }},
    {"AggregateOverflow",
     R"((module game :header "Game.h"
        (record Value (member items std::uint64_t :count 18446744073709551615))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {}, true)};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_FALSE(context.storage.components.extent_bytes[result.members.begin]);
         EXPECT_EQ(result, (AggregateLayout{.members = {result.members.begin, 1}}));
     }},
    {"RecordTargetsAndWorkload",
     R"((module game :header "Game.h"
        (record Value (member small std::uint8_t) (member wide std::uint32_t))))",
     [](AnalysisContext& context) {
         auto const type{context.document.find_type("game", "Value").value()};
         auto const abi{AbiProfile::host_common()};
         auto wide{abi};
         wide.set("std::uint32_t",
                  {.size_bytes = 8,
                   .alignment_bytes = 8,
                   .integer_signed = false,
                   .unsigned_value_bits = 32,
                   .provenance = {}});
         AccessSet const accesses{{"wide", AccessOperation::read}};
         std::array const requests{
             AnalysisRequest{.type = type,
                             .abi = &abi,
                             .element_count = 100,
                             .multiplicity = 3,
                             .accesses = &accesses},
             AnalysisRequest{.type = type, .abi = &wide, .element_count = 100}};
         analyze_requests(context, requests);
         auto const& storage{context.storage};
         EXPECT_EQ(*storage.types.aggregate[storage.runs[1].analysis.value]->size_bytes -
                       *storage.types.aggregate[storage.runs[0].analysis.value]->size_bytes,
                   8);
         EXPECT_EQ(*storage.totals[1].total_storage_bytes - *storage.totals[0].total_storage_bytes,
                   800);
         auto const access{storage.runs[0].access->value};
         EXPECT_EQ(storage.workloads.bytes[access],
                   (PayloadFootprint{.useful = 400,
                                     .read = 400,
                                     .write = 0,
                                     .logical_read = 1200,
                                     .logical_write = 0}));
         AccessSet const missing{{"missing", AccessOperation::read}};
         run_analysis(context, "Value", {.accesses = &missing}, true);
     }},
    {"UnionSlackAndDistribution",
     R"((module game :header "Game.h"
        (union Value (alternative word std::uint32_t)
          (alternative bytes std::uint8_t :count 10))))",
     [](AnalysisContext& context) {
         std::array const entries{UnionDistributionEntry{"word", 3},
                                  UnionDistributionEntry{"bytes", 1}};
         auto const run{
             run_analysis(context, "Value", {.element_count = 100, .union_distribution = entries})};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_EQ(result,
                   (AggregateLayout{.members = {result.members.begin, 2},
                                    .size_bytes = 12,
                                    .alignment_bytes = 4,
                                    .tail_padding_bytes = 2,
                                    .largest_alternative_bytes = 10}));
         EXPECT_EQ(context.storage.components.slack_bytes[result.members.begin], 8);
         EXPECT_EQ(context.storage.components.slack_bytes[result.members.begin + 1], 2);
         auto const distribution{run.distribution->value};
         auto const& values{context.storage.distributions};
         EXPECT_EQ(values.expected_extent_bytes[distribution], 5.5L);
         EXPECT_EQ(values.expected_slack_bytes[distribution], 6.5L);
         EXPECT_EQ(values.expected_selected_extent_bytes[distribution], 550.0L);
     }},
    {"NestedUnion",
     R"((module game :header "Game.h"
        (record Pair (member x double) (member y double))
        (union Value (alternative pair Pair) (alternative byte std::uint8_t))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(
             context, "Value", {.element_count = std::numeric_limits<std::uint64_t>::max()}, true)};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_EQ(result.size_bytes, 16);
         EXPECT_EQ(result.alignment_bytes, 8);
         EXPECT_FALSE(context.storage.totals[0].total_storage_bytes);
     }},
    {"TaggedUnion",
     R"((module game :header "Game.h"
        (enum Kind std::uint8_t :count Count
          (value Small) (value Bytes) (value Spare) (value Invalid :sentinel true) (value Count))
        (tagged-union Value :discriminant Kind
          (alternative small std::uint32_t :tag Small)
          (alternative bytes std::uint8_t :tag Bytes :count 10))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 100})};
         auto const& result{context.storage.types.aggregate[run.analysis.value].value()};
         EXPECT_EQ(result.payload_offset_bytes, 4);
         EXPECT_EQ(result.payload_size_bytes, 12);
         EXPECT_EQ(result.internal_padding_bytes, 3);
         EXPECT_EQ(result.size_bytes, 16);
         EXPECT_EQ(context.storage.totals[0].total_storage_bytes, 1600);
         auto const& tags{context.storage.tags};
         EXPECT_TRUE(tags.mapped[result.tags.begin]);
         EXPECT_TRUE(tags.mapped[result.tags.begin + 1]);
         EXPECT_EQ(tags.name[result.tags.begin + 2], "Spare");
         EXPECT_FALSE(tags.mapped[result.tags.begin + 2]);
         EXPECT_EQ(tags.name[result.tags.begin + 3], "Invalid");
         EXPECT_TRUE(tags.sentinel[result.tags.begin + 3]);
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         AggregateAnalysisTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

}
}
