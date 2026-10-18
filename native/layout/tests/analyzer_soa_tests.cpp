#include "analyzer_test_fixtures.hpp"

namespace ioj::layout {
namespace analyzer_soa_tests {

class SoaAnalysisTest : public testing::TestWithParam<AnalysisCase> {};

TEST_P(SoaAnalysisTest, ReportsColumnsAllocationsAndAccess) {
    auto const& test{GetParam()};
    auto const document{Document::from_source(test.source)};
    ASSERT_TRUE(document.valid());
    test.verify(document);
}

AnalysisCase const cases[]{
    {"GeneratedRecordColumn",
     R"((module game :header "Game.h" :backend standard-library
        (record Pair (member x float) (member y float))
        (struct Value (member pairs array Pair))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_soa(doc.types(),
                                                 doc.find_type("game", "Value").value(),
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 3)};
         EXPECT_EQ(result.total_payload_bytes, 24);
         EXPECT_EQ(result.columns.front().type_facts->alignment_bytes, alignof(float));
     }},
    {"UnknownMiddleColumn",
     R"((module game :header "Game.h" :backend standard-library
        (struct Value (member first array float) (member unknown array Opaque)
          (member last array double))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_soa(doc.types(),
                                                 doc.find_type("game", "Value").value(),
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 3,
                                                 SoaAllocationStrategy::aligned_contiguous)};
         EXPECT_EQ(result.columns[0].allocation_offset_bytes, 0);
         EXPECT_FALSE(result.columns[1].allocation_offset_bytes);
         EXPECT_FALSE(result.columns[2].allocation_offset_bytes);
         EXPECT_EQ(result.columns[2].total_bytes, 24);
         EXPECT_FALSE(result.total_allocation_bytes);
     }},
    {"AllocationStrategies",
     R"((module game :header "Game.h" :backend standard-library
        (struct Value (member small array std::uint8_t) (member wide array std::uint32_t))))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         auto const abi{AbiProfile::host_common()};
         auto const separate{Analyzer::analyze_soa(doc.types(), type, Variant{}, abi, 3)};
         auto const contiguous{Analyzer::analyze_soa(
             doc.types(), type, Variant{}, abi, 3, SoaAllocationStrategy::aligned_contiguous)};
         EXPECT_EQ(separate.allocation_count, 2);
         EXPECT_EQ(separate.total_payload_bytes, 15);
         EXPECT_EQ(contiguous.allocation_count, 1);
         EXPECT_EQ(contiguous.columns[1].allocation_offset_bytes, 4);
         EXPECT_EQ(contiguous.columns[1].padding_before_bytes, 1);
         EXPECT_EQ(contiguous.total_allocation_bytes, 16);
     }},
    {"ColumnOverflow",
     R"((module game :header "Game.h" :backend standard-library
        (struct Value (member values array double))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_soa(doc.types(),
                                                 doc.find_type("game", "Value").value(),
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 std::numeric_limits<std::uint64_t>::max())};
         EXPECT_FALSE(result.total_payload_bytes);
         EXPECT_FALSE(result.columns[0].total_bytes);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"TargetComparison",
     R"((module game :header "Game.h" :backend standard-library
        (struct Value (member small array std::uint8_t) (member wide array std::uint32_t))))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         auto wide{AbiProfile::host_common()};
         wide.set("std::uint32_t",
                  {.size_bytes = 8,
                   .alignment_bytes = 8,
                   .integer_signed = false,
                   .unsigned_value_bits = 32,
                   .provenance = {}});
         auto const first{
             Analyzer::analyze_soa(doc.types(), type, Variant{}, AbiProfile::host_common(), 3)};
         auto const second{Analyzer::analyze_soa(doc.types(), type, Variant{}, wide, 3)};
         auto const comparison{Analyzer::compare_soa_targets(first, second)};
         EXPECT_EQ(comparison.columns[1].total_byte_delta->magnitude, 12);
         auto const unknown{
             Analyzer::analyze_soa(doc.types(), type, Variant{}, AbiProfile{"unknown"}, 3)};
         EXPECT_FALSE(Analyzer::compare_soa_targets(first, unknown).columns[1].total_byte_delta);
     }},
    {"SelectedWorkload",
     R"((module game :header "Game.h" :backend standard-library
        (struct Value (member x array float) (member y array float) (member z array float))))",
     [](Document const& doc) {
         auto const abi{AbiProfile::host_common()};
         auto const result{Analyzer::analyze_soa(
             doc.types(), doc.find_type("game", "Value").value(), Variant{}, abi, 100)};
         std::array const accesses{AccessIntent{"x", AccessOperation::read}};
         auto const access{Analyzer::analyze_soa_access(result, accesses, abi, 10, 3)};
         EXPECT_EQ(access.useful_bytes, 40);
         EXPECT_EQ(access.logical_read_useful_bytes, 120);
         EXPECT_EQ(access.unselected_payload_bytes, 80);
         EXPECT_EQ(access.capacity_slack_payload_bytes, 1080);
         EXPECT_EQ(access.minimum_cache_lines_touched, 1);
         auto const too_many{Analyzer::analyze_soa_access(result, accesses, abi, 101)};
         EXPECT_FALSE(too_many.diagnostics.empty());
         std::array const missing{AccessIntent{"missing", AccessOperation::read}};
         EXPECT_FALSE(Analyzer::analyze_soa_access(result, missing, abi, 10).diagnostics.empty());
     }},
    {"RecordSoaComparison",
     R"((module game :header "Game.h" :backend standard-library
        (record Row (member x float) (member y float) (member z float))
        (struct Columns (member x array float) (member y array float) (member z array float))))",
     [](Document const& doc) {
         auto const abi{AbiProfile::host_common()};
         auto const record{
             Analyzer::analyze_record(doc.types(), doc.find_type("game", "Row").value(), abi, 100)};
         auto const soa{Analyzer::analyze_soa(
             doc.types(), doc.find_type("game", "Columns").value(), Variant{}, abi, 100)};
         std::array const accesses{AccessIntent{"x", AccessOperation::read}};
         auto const record_access{Analyzer::analyze_record_access(record, accesses, abi)};
         auto const soa_access{Analyzer::analyze_soa_access(soa, accesses, abi, 100)};
         EXPECT_EQ(record_access.useful_bytes, soa_access.useful_bytes);
         EXPECT_GT(record_access.cache_lines_touched.value(),
                   soa_access.minimum_cache_lines_touched.value());
         auto const comparison{Analyzer::compare_record_soa_access(record_access, soa_access)};
         EXPECT_TRUE(comparison.diagnostics.empty());
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         SoaAnalysisTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

class SoaCapacityTest : public testing::TestWithParam<std::uint64_t> {};

TEST_P(SoaCapacityTest, ScalesPayloadAndMemoryRegions) {
    auto const capacity{GetParam()};
    auto const doc{Document::from_source(R"((module game :header "Game.h" :backend standard-library
        (struct Value (member x array float) (member y array float) (member z array float))))")};
    ASSERT_TRUE(doc.valid());
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4096,
                          .l1_data_cache_bytes = 4096,
                          .l2_cache_bytes = 8192,
                          .l3_cache_bytes = 16384,
                          .provenance = "test memory"});
    auto const result{Analyzer::analyze_soa(
        doc.types(), doc.find_type("game", "Value").value(), Variant{}, abi, capacity)};
    EXPECT_EQ(result.bytes_per_logical_element, 12);
    EXPECT_EQ(result.total_payload_bytes, 12 * capacity);
    EXPECT_EQ(result.columns[0].minimum_cache_lines, (4 * capacity + 63) / 64);
    EXPECT_EQ(result.minimum_pages, 3 * ((4 * capacity + 4095) / 4096));
    EXPECT_EQ(result.cache_capacity.fits_l1_data, 12 * capacity <= 4096);
    EXPECT_EQ(result.cache_capacity.fits_l2, 12 * capacity <= 8192);
    EXPECT_EQ(result.cache_capacity.fits_l3, 12 * capacity <= 16384);
}

INSTANTIATE_TEST_SUITE_P(
    Capacities,
    SoaCapacityTest,
    testing::Values(std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{4096}, std::uint64_t{65536}));

}
}
