#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/resolved_variant.hpp>

namespace ioj::layout::analyzer_soa_tests {

class SoaAnalysisTest : public AnalysisTest {};

TEST_P(SoaAnalysisTest, ReportsColumnsAllocationsAndAccess) {
    verify_case();
}

AnalysisCase const cases[]{
    {"GeneratedRecordColumn",
     R"((module game :header "Game.h"
        (record Pair (member x float) (member y float)
    (projections (cpp)))
        (struct Value (member pairs array Pair))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.default_capacity = 3})};
         auto const& result{context.storage.types.soa()[run.analysis.value].value()};
         EXPECT_EQ(result.total_payload_bytes, 512);
         EXPECT_EQ(context.storage.physical_facts.alignment_bytes()
                       [context.storage.columns.type_facts()[result.columns.begin]->value],
                   alignof(float));
     }},
    {"UnknownMiddleColumn",
     R"((module game :header "Game.h"
        (struct Value (member first array float) (member unknown array Opaque)
          (member last array double))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.default_capacity = 3}, true)};
         auto const& result{context.storage.types.soa()[run.analysis.value].value()};
         auto const& columns{context.storage.columns};
         EXPECT_EQ(columns.allocation_offset_bytes()[result.columns.begin], 0);
         EXPECT_FALSE(columns.allocation_offset_bytes()[result.columns.begin + 1]);
         EXPECT_FALSE(columns.allocation_offset_bytes()[result.columns.begin + 2]);
         EXPECT_EQ(columns.total_bytes()[result.columns.begin + 2], 512);
         EXPECT_FALSE(result.total_allocation_bytes);
     }},
    {"SingleAllocationLayout",
     R"((module game :header "Game.h"
        (struct Value (member small array std::uint8_t) (member wide array std::uint32_t))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.default_capacity = 3})};
         auto const& storage{context.storage};
         auto const& result{storage.types.soa()[run.analysis.value].value()};
         SoaLayout const expected{.columns = {result.columns.begin, 2},
                                  .capacity = 64,
                                  .allocation_count = 1,
                                  .bytes_per_logical_element = 5,
                                  .total_payload_bytes = 320,
                                  .total_allocation_bytes = 512,
                                  .total_alignment_padding_bytes = 192,
                                  .allocation_alignment_bytes = 64};
         EXPECT_EQ(result, expected);
         EXPECT_EQ(storage.columns.allocation_offset_bytes()[result.columns.begin + 1], 256);
         EXPECT_EQ(storage.columns.padding_before_bytes()[result.columns.begin + 1], 192);
     }},
    {"ColumnOverflow",
     R"((module game :header "Game.h"
        (struct Value (member values array double))))",
     [](AnalysisContext& context) {
         auto const run{
             run_analysis(context,
                          "Value",
                          {.default_capacity = std::numeric_limits<std::uint64_t>::max()},
                          true)};
         auto const& result{context.storage.types.soa()[run.analysis.value].value()};
         EXPECT_FALSE(result.total_payload_bytes);
         EXPECT_FALSE(context.storage.columns.total_bytes()[result.columns.begin]);
     }},
    {"TargetComparison",
     R"((module game :header "Game.h"
        (struct Value (member small array std::uint8_t) (member wide array std::uint32_t))))",
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
         AbiProfile const unknown{"unknown"};
         std::array const requests{
             AnalysisRequest{.type = type, .abi = &abi, .default_capacity = 3},
             AnalysisRequest{.type = type, .abi = &wide, .default_capacity = 3},
             AnalysisRequest{.type = type, .abi = &unknown, .default_capacity = 3}};
         analyze_requests(context, requests, true);
         auto const& storage{context.storage};
         auto const first{storage.types.soa()[storage.runs[0].analysis.value]->columns.begin + 1};
         auto const second{storage.types.soa()[storage.runs[1].analysis.value]->columns.begin + 1};
         auto const missing{storage.types.soa()[storage.runs[2].analysis.value]->columns.begin + 1};
         EXPECT_EQ(*storage.columns.total_bytes()[second] - *storage.columns.total_bytes()[first],
                   256);
         EXPECT_FALSE(storage.columns.total_bytes()[missing]);
     }},
    {"SelectedWorkload",
     R"((module game :header "Game.h"
        (struct Value (member x array float) (member y array float) (member z array float))))",
     [](AnalysisContext& context) {
         AccessSet const accesses{{"x", AccessOperation::read}};
         auto const run{run_analysis(context,
                                     "Value",
                                     {.element_count = 10,
                                      .default_capacity = 100,
                                      .multiplicity = 3,
                                      .accesses = &accesses})};
         auto const row{run.access->value};
         auto const& access{context.storage.workloads};
         EXPECT_EQ(
             access.bytes()[row],
             (PayloadFootprint{
                 .useful = 40, .read = 40, .write = 0, .logical_read = 120, .logical_write = 0}));
         EXPECT_EQ(access.unselected_payload_bytes()[row], 80);
         EXPECT_EQ(access.capacity_slack_bytes()[row], 1416);
         EXPECT_EQ(access.cache()[row].count, 1);
         run_analysis(context,
                      "Value",
                      {.element_count = 129, .default_capacity = 100, .accesses = &accesses},
                      true);
         AccessSet const missing{{"missing", AccessOperation::read}};
         run_analysis(context,
                      "Value",
                      {.element_count = 10, .default_capacity = 100, .accesses = &missing},
                      true);
     }},
    {"RecordSoaComparison",
     R"((module game :header "Game.h"
        (record Row (member x float) (member y float) (member z float)
    (projections (cpp)))
        (struct Columns (member x array float) (member y array float) (member z array float))))",
     [](AnalysisContext& context) {
         auto const& doc{context.document};
         auto const abi{AbiProfile::host_common()};
         AccessSet const accesses{{"x", AccessOperation::read}};
         std::array const requests{AnalysisRequest{.type = doc.find_type("game", "Row").value(),
                                                   .abi = &abi,
                                                   .element_count = 100,
                                                   .accesses = &accesses},
                                   AnalysisRequest{.type = doc.find_type("game", "Columns").value(),
                                                   .abi = &abi,
                                                   .element_count = 100,
                                                   .default_capacity = 100,
                                                   .accesses = &accesses}};
         analyze_requests(context, requests);
         auto const record{context.storage.runs[0].access->value};
         auto const soa{context.storage.runs[1].access->value};
         auto const& access{context.storage.workloads};
         EXPECT_EQ(access.bytes()[record].useful, access.bytes()[soa].useful);
         EXPECT_GT(access.cache()[record].count.value(), access.cache()[soa].count.value());
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         SoaAnalysisTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

class SoaCapacityTest : public testing::TestWithParam<std::uint64_t> {};

TEST_P(SoaCapacityTest, ScalesPayloadAndMemoryRegions) {
    EditorSession editor;
    auto const capacity{GetParam()};
    auto const doc{editor.from_source(R"((module game :header "Game.h"
        (struct Value (member x array float) (member y array float) (member z array float))))")};
    ASSERT_TRUE(doc.valid());
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4096,
                          .l1_data_cache_bytes = 4096,
                          .l2_cache_bytes = 8192,
                          .l3_cache_bytes = 16384,
                          .provenance = "test memory"});
    AnalysisStorage storage;
    AnalysisContext context{doc, storage};
    AccessSet const accesses{{"x", AccessOperation::read}};
    auto const run{run_analysis(context,
                                "Value",
                                {.abi = &abi,
                                 .element_count = capacity,
                                 .default_capacity = capacity,
                                 .accesses = &accesses})};
    auto const& result{storage.types.soa()[run.analysis.value].value()};
    EXPECT_EQ(result.bytes_per_logical_element, 12);
    auto const reserved_capacity{(capacity + 63) / 64 * 64};
    auto const allocation_bytes{capacity == 0 ? 0 : 12 * reserved_capacity + 384};
    EXPECT_EQ(result.capacity, reserved_capacity);
    EXPECT_EQ(result.total_payload_bytes, 12 * reserved_capacity);
    EXPECT_EQ(result.total_allocation_bytes, allocation_bytes);
    EXPECT_EQ(result.allocation_count, capacity == 0 ? 0 : 1);
    EXPECT_EQ(storage.workloads.cache()[run.access->value].count, (4 * capacity + 63) / 64);
    EXPECT_EQ(storage.totals[0].minimum_pages, (allocation_bytes + 4095) / 4096);
    EXPECT_EQ(storage.totals[0].cache_capacity.fits_l1_data, allocation_bytes <= 4096);
    EXPECT_EQ(storage.totals[0].cache_capacity.fits_l2, allocation_bytes <= 8192);
    EXPECT_EQ(storage.totals[0].cache_capacity.fits_l3, allocation_bytes <= 16384);
}

INSTANTIATE_TEST_SUITE_P(
    Capacities,
    SoaCapacityTest,
    testing::Values(std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{4096}, std::uint64_t{65536}));

} // namespace ioj::layout::analyzer_soa_tests
