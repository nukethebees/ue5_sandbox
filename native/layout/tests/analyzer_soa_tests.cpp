#include "analyzer_test_fixtures.hpp"

namespace ioj::layout {
namespace {

TEST(SoaAnalyzer, ReportsSixFloatPayloadAcrossCapacities) {
    auto const fixture{soa_type()};
    auto const abi{AbiProfile::host_common()};
    for (auto const capacity : {std::uint64_t{0},
                                std::uint64_t{1},
                                std::uint64_t{4'096},
                                std::uint64_t{16'384},
                                std::uint64_t{65'536}}) {
        auto const analysis{
            Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, capacity)};
        EXPECT_EQ(analysis.bytes_per_logical_element, 24);
        EXPECT_EQ(analysis.total_payload_bytes, 24 * capacity);
        ASSERT_EQ(analysis.columns.size(), 6);
        EXPECT_EQ(analysis.columns[0].total_bytes, 4 * capacity);
        EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 16);
        EXPECT_EQ(analysis.columns[0].minimum_cache_lines,
                  (4 * capacity) / 64 + ((4 * capacity) % 64 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.columns[0].complete_elements_per_page, 1'024);
        EXPECT_EQ(analysis.columns[0].minimum_pages,
                  (4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1));
        EXPECT_EQ(analysis.minimum_pages,
                  6 * ((4 * capacity) / 4'096 + ((4 * capacity) % 4'096 == 0 ? 0 : 1)));
    }
}

TEST(SoaAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});

    auto const first{Analyzer::analyze_soa(fixture.types,
                                           fixture.type,
                                           Variant{},
                                           first_profile,
                                           3,
                                           SoaAllocationStrategy::aligned_contiguous)};
    auto const second{Analyzer::analyze_soa(fixture.types,
                                            fixture.type,
                                            Variant{},
                                            second_profile,
                                            3,
                                            SoaAllocationStrategy::aligned_contiguous)};
    auto const comparison{Analyzer::compare_soa_targets(first, second)};

    EXPECT_TRUE(comparison.compatible);
    ASSERT_EQ(comparison.columns.size(), 2);
    EXPECT_EQ(comparison.columns[0].element_size_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[0].allocation_offset_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[1].name, "wide");
    EXPECT_EQ(comparison.columns[1].element_size_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].element_alignment_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].total_byte_delta->magnitude, 12);
    EXPECT_EQ(comparison.columns[1].first.allocation_offset_bytes, 4);
    EXPECT_EQ(comparison.columns[1].second.allocation_offset_bytes, 8);
    EXPECT_EQ(comparison.columns[1].allocation_offset_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[1].first.padding_before_bytes, 1);
    EXPECT_EQ(comparison.columns[1].second.padding_before_bytes, 5);
    EXPECT_EQ(comparison.columns[1].padding_before_delta->magnitude, 4);
    EXPECT_EQ(comparison.bytes_per_logical_element_delta->magnitude, 4);
    EXPECT_EQ(comparison.total_payload_delta->magnitude, 12);
    EXPECT_EQ(comparison.total_allocation_delta->magnitude, 16);
    EXPECT_EQ(comparison.total_alignment_padding_delta->magnitude, 4);
    EXPECT_EQ(comparison.allocation_alignment_delta->magnitude, 4);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_soa_targets(first, first)};
    EXPECT_TRUE(identical.compatible);
    ASSERT_EQ(identical.columns.size(), 2);
    EXPECT_EQ(identical.columns[1].element_size_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.total_allocation_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(SoaAnalyzer, PreservesUnknownTargetFactsAndSideDiagnostics) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const known{Analyzer::analyze_soa(fixture.types,
                                           fixture.type,
                                           Variant{},
                                           AbiProfile::host_common(),
                                           3,
                                           SoaAllocationStrategy::aligned_contiguous)};
    auto const unknown{Analyzer::analyze_soa(fixture.types,
                                             fixture.type,
                                             Variant{},
                                             AbiProfile{"unknown target"},
                                             3,
                                             SoaAllocationStrategy::aligned_contiguous)};

    auto const comparison{Analyzer::compare_soa_targets(known, unknown)};

    EXPECT_TRUE(comparison.compatible);
    ASSERT_EQ(comparison.columns.size(), 2);
    EXPECT_FALSE(comparison.columns[1].element_size_delta.has_value());
    EXPECT_FALSE(comparison.columns[1].allocation_offset_delta.has_value());
    EXPECT_FALSE(comparison.total_allocation_delta.has_value());
    EXPECT_TRUE(std::ranges::any_of(comparison.diagnostics, [](Diagnostic const& diagnostic) {
        return diagnostic.message.starts_with("Second target SoA layout:");
    }));
}

TEST(SoaAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialColumns) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const analysis{Analyzer::analyze_soa(fixture.types,
                                              fixture.type,
                                              Variant{},
                                              AbiProfile::host_common(),
                                              3,
                                              SoaAllocationStrategy::aligned_contiguous)};
    auto second{analysis};
    second.columns.back().physical_type = "std::uint64_t";

    auto comparison{Analyzer::compare_soa_targets(analysis, second)};
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same unique ordered columns"),
              std::string::npos);

    second = analysis;
    second.capacity = 4;
    comparison = Analyzer::compare_soa_targets(analysis, second);
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different capacities"),
              std::string::npos);

    second = analysis;
    second.allocation_strategy = SoaAllocationStrategy::separate_columns;
    comparison = Analyzer::compare_soa_targets(analysis, second);
    EXPECT_FALSE(comparison.compatible);
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different allocation strategies"),
              std::string::npos);
}

TEST(SoaAnalyzer, AppliesCapacityAndColumnTypeOverrides) {
    auto const fixture{soa_type()};
    Variant variant;
    variant.overrides.capacities[fixture.type] = 4'096;
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "min_xs"}] = "double";

    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 65'536)};

    EXPECT_EQ(analysis.capacity, 4'096);
    EXPECT_TRUE(analysis.capacity_overridden);
    EXPECT_EQ(analysis.bytes_per_logical_element, 28);
    EXPECT_EQ(analysis.total_payload_bytes, 28 * 4'096);
    EXPECT_EQ(analysis.columns[0].type_facts->size_bytes, 8);
    EXPECT_EQ(analysis.columns[0].elements_per_cache_line, 8);
}

TEST(SoaAnalyzer, ModelsSeparateAndAlignedContiguousAllocations) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}, {"medium", "std::uint16_t"}})};
    auto const abi{AbiProfile::host_common()};

    auto const separate{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::separate_columns)};
    EXPECT_EQ(separate.allocation_count, 3);
    EXPECT_EQ(separate.total_payload_bytes, 21);
    EXPECT_EQ(separate.total_allocation_bytes, 21);
    EXPECT_EQ(separate.total_alignment_padding_bytes, 0);
    EXPECT_FALSE(separate.allocation_alignment_bytes.has_value());
    EXPECT_TRUE(std::ranges::all_of(separate.columns, [](SoaColumnAnalysis const& column) {
        return !column.allocation_offset_bytes.has_value() &&
               !column.padding_before_bytes.has_value();
    }));

    auto const contiguous{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_EQ(contiguous.allocation_count, 1);
    EXPECT_EQ(contiguous.total_payload_bytes, 21);
    EXPECT_EQ(contiguous.total_allocation_bytes, 22);
    EXPECT_EQ(contiguous.total_alignment_padding_bytes, 1);
    EXPECT_EQ(contiguous.allocation_alignment_bytes, 4);
    EXPECT_EQ(contiguous.cache_line_bytes, 64);
    EXPECT_EQ(contiguous.page_bytes, 4'096);
    ASSERT_EQ(contiguous.columns.size(), 3);
    EXPECT_EQ(contiguous.columns[0].allocation_offset_bytes, 0);
    EXPECT_EQ(contiguous.columns[0].padding_before_bytes, 0);
    EXPECT_EQ(contiguous.columns[1].allocation_offset_bytes, 4);
    EXPECT_EQ(contiguous.columns[1].padding_before_bytes, 1);
    EXPECT_EQ(contiguous.columns[2].allocation_offset_bytes, 16);
    EXPECT_EQ(contiguous.columns[2].padding_before_bytes, 0);

    auto const empty_capacity{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 0, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_EQ(empty_capacity.allocation_count, 0);
    EXPECT_EQ(empty_capacity.total_allocation_bytes, 0);
    EXPECT_EQ(empty_capacity.total_alignment_padding_bytes, 0);
}

TEST(SoaAnalyzer, DerivesRelationshipTargetFactsPerVariantAndAllocationStrategy) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint64_t"}})};
    auto variant{Variant{}};
    variant.overrides.capacities[fixture.type] = 3;

    auto const separate{
        Analyzer::derive_relationship_target_facts(fixture.types,
                                                   variant,
                                                   AbiProfile::host_common(),
                                                   100,
                                                   SoaAllocationStrategy::separate_columns)};
    auto const contiguous{
        Analyzer::derive_relationship_target_facts(fixture.types,
                                                   variant,
                                                   AbiProfile::host_common(),
                                                   100,
                                                   SoaAllocationStrategy::aligned_contiguous)};

    ASSERT_EQ(separate.size(), 1);
    ASSERT_EQ(contiguous.size(), 1);
    EXPECT_EQ(separate.front().target, fixture.type);
    EXPECT_EQ(separate.front().element_capacity, 3);
    EXPECT_EQ(separate.front().byte_extent, 27);
    EXPECT_EQ(contiguous.front().target, fixture.type);
    EXPECT_EQ(contiguous.front().element_capacity, 3);
    EXPECT_EQ(contiguous.front().byte_extent, 32);
}

TEST(SoaAnalyzer, KeepsUnknownRelationshipTargetByteExtentUnknown) {
    auto const fixture{soa_type({{"values", "std::uint8_t"}})};
    auto const facts{Analyzer::derive_relationship_target_facts(
        fixture.types, Variant{}, AbiProfile{"unknown physical facts"}, 64)};

    ASSERT_EQ(facts.size(), 1);
    EXPECT_EQ(facts.front().target, fixture.type);
    EXPECT_EQ(facts.front().element_capacity, 64);
    EXPECT_FALSE(facts.front().byte_extent.has_value());
}

TEST(SoaAnalyzer, KeepsIncompleteContiguousAllocationUnknown) {
    auto const fixture{soa_type({{"unknown", "UnknownUserType"}})};
    auto const abi{AbiProfile::host_common()};
    auto const unknown{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 3, SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_FALSE(unknown.total_allocation_bytes.has_value());
    EXPECT_FALSE(unknown.total_alignment_padding_bytes.has_value());
    EXPECT_FALSE(unknown.allocation_alignment_bytes.has_value());
    EXPECT_FALSE(unknown.diagnostics.empty());

    auto const overflow{soa_type({{"wide", "std::uint64_t"}})};
    auto const overflowed{Analyzer::analyze_soa(overflow.types,
                                                overflow.type,
                                                Variant{},
                                                abi,
                                                std::numeric_limits<std::uint64_t>::max(),
                                                SoaAllocationStrategy::aligned_contiguous)};
    EXPECT_FALSE(overflowed.total_allocation_bytes.has_value());
    EXPECT_FALSE(overflowed.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsCacheLineTilingForNonDivisibleAndOversizedElements) {
    auto const fixture{soa_type({{"three", "three_bytes"}, {"wide_values", "wide"}})};
    AbiProfile abi{"test"};
    abi.set("three_bytes",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});
    abi.set("wide",
            {.size_bytes = 80,
             .alignment_bytes = 16,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test profile"});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1)};

    ASSERT_TRUE(analysis.columns[0].cache_line_tiling.has_value());
    auto const& three_bytes{*analysis.columns[0].cache_line_tiling};
    EXPECT_FALSE(three_bytes.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(three_bytes.complete_elements_from_line_start, 21);
    EXPECT_EQ(three_bytes.boundary_fragment_bytes, 1);
    EXPECT_EQ(three_bytes.minimum_cache_lines_per_element, 1);

    ASSERT_TRUE(analysis.columns[1].cache_line_tiling.has_value());
    auto const& wide{*analysis.columns[1].cache_line_tiling};
    EXPECT_FALSE(wide.exact_elements_per_cache_line.has_value());
    EXPECT_EQ(wide.complete_elements_from_line_start, 0);
    EXPECT_EQ(wide.boundary_fragment_bytes, 0);
    EXPECT_EQ(wide.minimum_cache_lines_per_element, 2);
}

TEST(SoaAnalyzer, LeavesCacheStatisticsUnknownWithoutTargetFact) {
    auto const fixture{soa_type({{"values", "four"}})};
    AbiProfile abi{"unknown memory"};
    abi.set("four",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = {},
             .provenance = {}});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};

    EXPECT_EQ(analysis.columns[0].total_bytes, 400);
    EXPECT_FALSE(analysis.columns[0].minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.columns[0].cache_line_tiling.has_value());
    EXPECT_FALSE(analysis.columns[0].minimum_pages.has_value());
    EXPECT_FALSE(analysis.minimum_pages.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, UnknownTypesRemainUnknown) {
    auto const fixture{soa_type({{"unknown", "UnknownUserType"}})};
    auto const analysis{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};

    EXPECT_FALSE(analysis.columns[0].type_facts.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsIntegerOverflow) {
    auto const fixture{soa_type({{"values", "huge"}})};
    AbiProfile abi{"test"};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = {}});

    auto const analysis{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 2)};

    EXPECT_FALSE(analysis.columns[0].total_bytes.has_value());
    EXPECT_FALSE(analysis.total_payload_bytes.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsSelectedColumnAccessFootprints) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1'000)};
    std::vector<std::string> const columns{"wide", "small", "wide"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 100)};

    EXPECT_EQ(access.column_names, (std::vector<std::string>{"wide", "small"}));
    ASSERT_EQ(access.columns.size(), 2);
    EXPECT_EQ(access.columns[0].name, "wide");
    EXPECT_EQ(access.columns[0].physical_type, "std::uint64_t");
    EXPECT_EQ(access.columns[0].element_bytes, 8);
    EXPECT_EQ(access.columns[0].useful_bytes, 800);
    EXPECT_EQ(access.columns[0].read_useful_bytes, 800);
    EXPECT_EQ(access.columns[0].write_useful_bytes, 0);
    EXPECT_EQ(access.columns[0].minimum_cache_lines, 13);
    EXPECT_EQ(access.columns[0].minimum_cache_bytes, 832);
    EXPECT_EQ(access.columns[0].non_payload_cache_bytes, 32);
    EXPECT_EQ(access.columns[0].minimum_pages, 1);
    EXPECT_EQ(access.columns[0].minimum_page_bytes, 4'096);
    EXPECT_EQ(access.columns[0].non_payload_page_bytes, 3'296);
    EXPECT_EQ(access.columns[0].allocated_capacity_payload_bytes, 8'000);
    EXPECT_EQ(access.columns[0].capacity_slack_payload_bytes, 7'200);
    EXPECT_EQ(access.columns[1].name, "small");
    EXPECT_EQ(access.columns[1].element_bytes, 1);
    EXPECT_EQ(access.columns[1].useful_bytes, 100);
    EXPECT_EQ(access.columns[1].read_useful_bytes, 100);
    EXPECT_EQ(access.columns[1].write_useful_bytes, 0);
    EXPECT_EQ(access.columns[1].minimum_cache_lines, 2);
    EXPECT_EQ(access.columns[1].minimum_cache_bytes, 128);
    EXPECT_EQ(access.columns[1].non_payload_cache_bytes, 28);
    EXPECT_EQ(access.columns[1].minimum_pages, 1);
    EXPECT_EQ(access.columns[1].minimum_page_bytes, 4'096);
    EXPECT_EQ(access.columns[1].non_payload_page_bytes, 3'996);
    EXPECT_EQ(access.columns[1].allocated_capacity_payload_bytes, 1'000);
    EXPECT_EQ(access.columns[1].capacity_slack_payload_bytes, 900);
    EXPECT_EQ(access.element_count, 100);
    ASSERT_EQ(access.accesses.size(), 2);
    EXPECT_EQ(access.accesses.front().operation, AccessOperation::read);
    EXPECT_EQ(access.useful_bytes, 900);
    EXPECT_EQ(access.read_useful_bytes, 900);
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_EQ(access.logical_read_useful_bytes, 900);
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.full_logical_payload_bytes, 1'300);
    EXPECT_EQ(access.unselected_payload_bytes, 400);
    EXPECT_EQ(access.allocated_capacity_payload_bytes, 13'000);
    EXPECT_EQ(access.capacity_slack_payload_bytes, 11'700);
    EXPECT_EQ(access.cache_line_bytes, 64);
    EXPECT_EQ(access.minimum_cache_lines_touched, 15);
    EXPECT_EQ(access.minimum_cache_bytes_touched, 960);
    EXPECT_EQ(access.non_payload_cache_bytes, 60);
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l1_data.has_value());
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l2.has_value());
    EXPECT_FALSE(access.minimum_cache_footprint_capacity.fits_l3.has_value());
    EXPECT_EQ(access.page_bytes, 4'096);
    EXPECT_EQ(access.minimum_pages_touched, 2);
    EXPECT_EQ(access.minimum_page_bytes_touched, 8'192);
    EXPECT_EQ(access.non_payload_page_bytes, 7'292);
    EXPECT_TRUE(access.diagnostics.empty());
}

TEST(SoaAnalyzer, ReportsAlignedSelectedColumnBoundaryCrossings) {
    auto const fixture{soa_type({{"even", "even"}, {"odd", "odd"}, {"wide", "wide"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("even",
            {.size_bytes = 2,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("odd",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 20)};
    std::vector<std::string> const columns{"even", "odd", "wide"};

    auto access{Analyzer::analyze_soa_access(soa, columns, abi, 10)};

    ASSERT_EQ(access.columns.size(), 3);
    EXPECT_EQ(access.columns[0].aligned_cache_line_straddling_elements, 0);
    EXPECT_EQ(access.columns[0].aligned_page_straddling_elements, 0);
    EXPECT_EQ(access.columns[1].aligned_cache_line_straddling_elements, 2);
    EXPECT_EQ(access.columns[1].aligned_page_straddling_elements, 2);
    EXPECT_EQ(access.columns[2].aligned_cache_line_straddling_elements, 10);
    EXPECT_EQ(access.columns[2].aligned_page_straddling_elements, 10);

    access = Analyzer::analyze_soa_access(soa, columns, abi, 0);
    for (auto const& column : access.columns) {
        EXPECT_EQ(column.aligned_cache_line_straddling_elements, 0);
        EXPECT_EQ(column.aligned_page_straddling_elements, 0);
    }

    abi.set_memory_facts({.cache_line_bytes = std::nullopt,
                          .page_bytes = std::nullopt,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test unknown"});
    access = Analyzer::analyze_soa_access(soa, columns, abi, 10);
    for (auto const& column : access.columns) {
        EXPECT_FALSE(column.aligned_cache_line_straddling_elements.has_value());
        EXPECT_FALSE(column.aligned_page_straddling_elements.has_value());
    }
}

TEST(SoaAnalyzer, ComparesAlignedSelectedColumnBoundaryCrossingsAcrossVariants) {
    auto const fixture{soa_type({{"value", "odd"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("odd",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});
    auto const baseline{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 20)};
    auto variant{Variant{}};
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "value"}] = "wide";
    auto const overridden{Analyzer::analyze_soa(fixture.types, fixture.type, variant, abi, 20)};
    std::vector<std::string> const columns{"value"};
    auto const first{Analyzer::analyze_soa_access(baseline, columns, abi, 10)};
    auto const second{Analyzer::analyze_soa_access(overridden, columns, abi, 10)};

    auto const comparison{Analyzer::compare_soa_access(first, second)};

    ASSERT_EQ(comparison.columns.size(), 1);
    auto const& column{comparison.columns.front()};
    EXPECT_EQ(column.first.aligned_cache_line_straddling_elements, 2);
    EXPECT_EQ(column.second.aligned_cache_line_straddling_elements, 10);
    EXPECT_EQ(column.aligned_cache_line_straddling_element_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(column.aligned_cache_line_straddling_element_delta->magnitude, 8);
    EXPECT_EQ(column.first.aligned_page_straddling_elements, 2);
    EXPECT_EQ(column.second.aligned_page_straddling_elements, 10);
    EXPECT_EQ(column.aligned_page_straddling_element_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(column.aligned_page_straddling_element_delta->magnitude, 8);
}

TEST(SoaAnalyzer, KeepsIncompleteSelectedColumnAccessUnknown) {
    auto const fixture{soa_type({{"known", "std::uint32_t"}, {"unknown", "UnknownUserType"}})};
    AbiProfile abi{"partial"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::vector<std::string> const columns{"known", "unknown", "missing"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 10)};

    EXPECT_EQ(access.column_names, (std::vector<std::string>{"known", "unknown"}));
    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.read_useful_bytes.has_value());
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_FALSE(access.full_logical_payload_bytes.has_value());
    EXPECT_FALSE(access.unselected_payload_bytes.has_value());
    EXPECT_FALSE(access.capacity_slack_payload_bytes.has_value());
    EXPECT_FALSE(access.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(access.minimum_pages_touched.has_value());
    EXPECT_FALSE(access.minimum_read_cache_lines_touched.has_value());
    EXPECT_EQ(access.minimum_write_cache_lines_touched, 0);
    EXPECT_FALSE(access.minimum_read_pages_touched.has_value());
    EXPECT_EQ(access.minimum_write_pages_touched, 0);
    EXPECT_FALSE(access.cache_line_bytes.has_value());
    EXPECT_FALSE(access.page_bytes.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    auto const empty{Analyzer::analyze_soa_access(soa, std::span<std::string const>{}, abi, 10)};
    EXPECT_TRUE(empty.column_names.empty());
    EXPECT_FALSE(empty.useful_bytes.has_value());
    EXPECT_FALSE(empty.diagnostics.empty());
}

TEST(SoaAnalyzer, DiagnosesSelectedColumnAccessOverflowAndExcessCapacity) {
    auto const fixture{soa_type({{"huge", "huge"}})};
    auto abi{AbiProfile::host_common()};
    abi.set("huge",
            {.size_bytes = std::numeric_limits<std::uint64_t>::max(),
             .alignment_bytes = 1,
             .integer_signed = std::nullopt,
             .unsigned_value_bits = std::nullopt,
             .provenance = "test"});
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1)};
    std::vector<std::string> const columns{"huge"};

    auto const access{Analyzer::analyze_soa_access(soa, columns, abi, 2)};

    EXPECT_FALSE(access.useful_bytes.has_value());
    EXPECT_FALSE(access.read_useful_bytes.has_value());
    EXPECT_EQ(access.write_useful_bytes, 0);
    EXPECT_FALSE(access.full_logical_payload_bytes.has_value());
    EXPECT_FALSE(access.capacity_slack_payload_bytes.has_value());
    EXPECT_FALSE(access.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(access.minimum_pages_touched.has_value());
    EXPECT_FALSE(access.diagnostics.empty());

    auto const write_access{
        Analyzer::analyze_soa_access(soa, columns, abi, 2, AccessOperation::write)};
    EXPECT_EQ(write_access.read_useful_bytes, 0);
    EXPECT_FALSE(write_access.write_useful_bytes.has_value());
}

TEST(SoaAnalyzer, ScalesLogicalAccessMultiplicityWithoutChangingFootprint) {
    auto const fixture{soa_type({{"wide", "std::uint64_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::vector<std::string> const columns{"wide"};

    auto access{
        Analyzer::analyze_soa_access(soa, columns, abi, 10, AccessOperation::read_write, 3)};
    EXPECT_EQ(access.multiplicity, 3);
    EXPECT_EQ(access.useful_bytes, 80);
    EXPECT_EQ(access.logical_read_useful_bytes, 240);
    EXPECT_EQ(access.logical_write_useful_bytes, 240);
    EXPECT_EQ(access.minimum_cache_lines_touched, 2);
    EXPECT_EQ(access.minimum_read_cache_lines_touched, 2);
    EXPECT_EQ(access.minimum_write_cache_lines_touched, 2);

    access = Analyzer::analyze_soa_access(
        soa, columns, abi, 1, AccessOperation::read, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(access.useful_bytes, 8);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_EQ(access.logical_write_useful_bytes, 0);
    EXPECT_EQ(access.minimum_cache_lines_touched, 1);
    EXPECT_FALSE(access.diagnostics.empty());

    access = Analyzer::analyze_soa_access(soa, columns, abi, 10, AccessOperation::read, 0);
    EXPECT_EQ(access.useful_bytes, 80);
    EXPECT_FALSE(access.logical_read_useful_bytes.has_value());
    EXPECT_FALSE(access.logical_write_useful_bytes.has_value());
    EXPECT_EQ(access.minimum_cache_lines_touched, 2);
    EXPECT_FALSE(access.diagnostics.empty());
}

TEST(SoaAnalyzer, ClassifiesIndividualColumnAccessOperations) {
    auto const fixture{soa_type(
        {{"wide", "std::uint64_t"}, {"small", "std::uint8_t"}, {"medium", "std::uint32_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::vector<AccessIntent> const accesses{
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "small", .operation = AccessOperation::read},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 2)};

    EXPECT_EQ(analysis.accesses, accesses);
    EXPECT_EQ(analysis.useful_bytes, 1'300);
    EXPECT_EQ(analysis.read_useful_bytes, 500);
    EXPECT_EQ(analysis.write_useful_bytes, 1'200);
    EXPECT_EQ(analysis.logical_read_useful_bytes, 1'000);
    EXPECT_EQ(analysis.logical_write_useful_bytes, 2'400);
    ASSERT_EQ(analysis.columns.size(), 3);
    EXPECT_EQ(analysis.columns[0].operation, AccessOperation::write);
    EXPECT_EQ(analysis.columns[1].operation, AccessOperation::read);
    EXPECT_EQ(analysis.columns[2].operation, AccessOperation::read_write);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 22);
    EXPECT_EQ(analysis.minimum_read_cache_lines_touched, 9);
    EXPECT_EQ(analysis.minimum_read_cache_bytes_touched, 576);
    EXPECT_EQ(analysis.minimum_write_cache_lines_touched, 20);
    EXPECT_EQ(analysis.minimum_write_cache_bytes_touched, 1'280);
    EXPECT_EQ(analysis.minimum_pages_touched, 3);
    EXPECT_EQ(analysis.minimum_read_pages_touched, 2);
    EXPECT_EQ(analysis.minimum_write_pages_touched, 2);
    EXPECT_EQ(analysis.minimum_read_page_bytes_touched, 8'192);
    EXPECT_EQ(analysis.minimum_write_page_bytes_touched, 8'192);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto same{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 2)};
    auto comparison{Analyzer::compare_soa_access(analysis, same)};
    EXPECT_TRUE(comparison.diagnostics.empty());
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    same.accesses.front().operation = AccessOperation::read;
    comparison = Analyzer::compare_soa_access(analysis, same);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());

    auto conflicting{accesses};
    conflicting.push_back({.name = "wide", .operation = AccessOperation::read});
    auto const normalized{Analyzer::analyze_soa_access(soa, conflicting, abi, 100, 2)};
    EXPECT_EQ(normalized.accesses, accesses);
    EXPECT_EQ(normalized.read_useful_bytes, 500);
    EXPECT_EQ(normalized.write_useful_bytes, 1'200);
    EXPECT_FALSE(normalized.diagnostics.empty());
}

TEST(SoaAnalyzer, UnionsSelectedAccessInAlignedContiguousBlock) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"wide", "std::uint64_t"}, {"medium", "std::uint32_t"}})};
    auto const abi{AbiProfile::host_common()};
    auto const soa{Analyzer::analyze_soa(fixture.types,
                                         fixture.type,
                                         Variant{},
                                         abi,
                                         10,
                                         SoaAllocationStrategy::aligned_contiguous)};
    std::vector<AccessIntent> const accesses{
        {.name = "small", .operation = AccessOperation::read},
        {.name = "wide", .operation = AccessOperation::write},
        {.name = "medium", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_soa_access(soa, accesses, abi, 2)};

    EXPECT_EQ(analysis.allocation_strategy, SoaAllocationStrategy::aligned_contiguous);
    EXPECT_TRUE(analysis.footprint_exact);
    EXPECT_EQ(analysis.allocation_count, 1);
    EXPECT_EQ(analysis.total_allocation_bytes, 136);
    EXPECT_EQ(analysis.alignment_padding_bytes, 6);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_cache_bytes_touched, 128);
    EXPECT_EQ(analysis.minimum_read_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_write_cache_lines_touched, 2);
    EXPECT_EQ(analysis.minimum_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_read_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_write_pages_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());

    auto variant{Variant{}};
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "wide"}] =
        "std::uint32_t";
    auto const compact_soa{Analyzer::analyze_soa(
        fixture.types, fixture.type, variant, abi, 10, SoaAllocationStrategy::aligned_contiguous)};
    auto const compact_access{Analyzer::analyze_soa_access(compact_soa, accesses, abi, 2)};
    auto const comparison{Analyzer::compare_soa_access(analysis, compact_access)};
    EXPECT_EQ(comparison.allocation_strategy, SoaAllocationStrategy::aligned_contiguous);
    EXPECT_TRUE(comparison.footprint_exact);
    EXPECT_EQ(comparison.first_allocation_count, 1);
    EXPECT_EQ(comparison.second_allocation_count, 1);
    EXPECT_EQ(comparison.first_total_allocation_bytes, 136);
    EXPECT_EQ(comparison.second_total_allocation_bytes, 92);
    EXPECT_EQ(comparison.total_allocation_byte_delta->magnitude, 44);
    EXPECT_EQ(comparison.alignment_padding_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 1);

    auto const excess{Analyzer::analyze_soa_access(soa, accesses, abi, 11)};
    EXPECT_FALSE(excess.footprint_exact);
    EXPECT_FALSE(excess.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(excess.minimum_pages_touched.has_value());
    EXPECT_FALSE(excess.diagnostics.empty());
}

TEST(SoaAnalyzer, UsesContiguousColumnOffsetsForBoundaryCrossings) {
    auto const fixture{soa_type({{"lead", "Lead"}, {"odd", "Odd"}})};
    auto abi{AbiProfile{"Offset target"}};
    abi.set("Lead",
            TypeFacts{.size_bytes = 8,
                      .alignment_bytes = 4,
                      .integer_signed = std::nullopt,
                      .unsigned_value_bits = std::nullopt,
                      .provenance = "Test target"});
    abi.set("Odd",
            TypeFacts{.size_bytes = 12,
                      .alignment_bytes = 4,
                      .integer_signed = std::nullopt,
                      .unsigned_value_bits = std::nullopt,
                      .provenance = "Test target"});
    abi.set_memory_facts({.cache_line_bytes = 16,
                          .page_bytes = 32,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "Test target"});
    std::array<std::string, 1> const selected{"odd"};

    auto const separate{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 1, SoaAllocationStrategy::separate_columns)};
    auto const separate_access{Analyzer::analyze_soa_access(separate, selected, abi, 1)};
    ASSERT_EQ(separate_access.columns.size(), 1);
    EXPECT_EQ(separate_access.columns.front().aligned_cache_line_straddling_elements, 0);

    auto const contiguous{Analyzer::analyze_soa(
        fixture.types, fixture.type, Variant{}, abi, 1, SoaAllocationStrategy::aligned_contiguous)};
    ASSERT_EQ(contiguous.columns[1].allocation_offset_bytes, 8);
    auto const contiguous_access{Analyzer::analyze_soa_access(contiguous, selected, abi, 1)};
    ASSERT_EQ(contiguous_access.columns.size(), 1);
    EXPECT_EQ(contiguous_access.columns.front().aligned_cache_line_straddling_elements, 1);
    EXPECT_EQ(contiguous_access.columns.front().aligned_page_straddling_elements, 0);
}

TEST(SoaAnalyzer, ComparesEquivalentRecordAndSoaAccessFacts) {
    auto const record_fixture{
        record_type({codegen::RecordSchema{.name = "Row",
                                           .members = {record_member("small", "std::uint8_t"),
                                                       record_member("medium", "std::uint32_t"),
                                                       record_member("wide", "std::uint64_t")},
                                           .export_specifier = std::nullopt}},
                    "Row")};
    auto const soa_fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 1'000,
                          .l2_cache_bytes = 960,
                          .l3_cache_bytes = 1'600,
                          .provenance = "test profile"});
    auto const record{
        Analyzer::analyze_record(record_fixture.types, record_fixture.type, abi, 100)};
    auto const soa{
        Analyzer::analyze_soa(soa_fixture.types, soa_fixture.type, Variant{}, abi, 1'000)};
    std::vector<std::string> const members{"small", "wide"};
    std::vector<AccessIntent> const accesses{{.name = "small", .operation = AccessOperation::read},
                                             {.name = "wide", .operation = AccessOperation::write}};
    auto const record_access{Analyzer::analyze_record_access(record, accesses, abi, 3)};
    auto const soa_access{Analyzer::analyze_soa_access(soa, accesses, abi, 100, 3)};

    auto const comparison{Analyzer::compare_record_soa_access(record_access, soa_access)};

    EXPECT_EQ(comparison.member_names, members);
    EXPECT_EQ(comparison.element_count, 100);
    EXPECT_EQ(comparison.accesses, accesses);
    EXPECT_EQ(comparison.multiplicity, 3);
    EXPECT_EQ(comparison.record.useful_bytes, 900);
    EXPECT_EQ(comparison.soa.useful_bytes, 900);
    EXPECT_EQ(comparison.record.read_useful_bytes, 100);
    EXPECT_EQ(comparison.soa.read_useful_bytes, 100);
    EXPECT_EQ(comparison.record.write_useful_bytes, 800);
    EXPECT_EQ(comparison.soa.write_useful_bytes, 800);
    EXPECT_EQ(comparison.record.logical_read_useful_bytes, 300);
    EXPECT_EQ(comparison.soa.logical_read_useful_bytes, 300);
    EXPECT_EQ(comparison.record.logical_write_useful_bytes, 2'400);
    EXPECT_EQ(comparison.soa.logical_write_useful_bytes, 2'400);
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.write_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.record.cache_lines, 25);
    EXPECT_EQ(comparison.record.cache_bytes, 1'600);
    EXPECT_EQ(comparison.soa.cache_lines, 15);
    EXPECT_EQ(comparison.soa.cache_bytes, 960);
    EXPECT_EQ(comparison.record.read_cache_bytes, 1'600);
    EXPECT_EQ(comparison.record.write_cache_bytes, 1'600);
    EXPECT_EQ(comparison.soa.read_cache_bytes, 128);
    EXPECT_EQ(comparison.soa.write_cache_bytes, 832);
    EXPECT_EQ(comparison.read_cache_byte_delta->magnitude, 1'472);
    EXPECT_EQ(comparison.write_cache_byte_delta->magnitude, 768);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.working_set_bytes, 1'600);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.working_set_bytes, 960);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l1_data, false);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l2, false);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l2, true);
    EXPECT_EQ(comparison.record_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.soa_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.record.pages, 1);
    EXPECT_EQ(comparison.record.page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.pages, 2);
    EXPECT_EQ(comparison.soa.page_bytes, 8'192);
    EXPECT_EQ(comparison.record.read_page_bytes, 4'096);
    EXPECT_EQ(comparison.record.write_page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.read_page_bytes, 4'096);
    EXPECT_EQ(comparison.soa.write_page_bytes, 4'096);
    EXPECT_EQ(comparison.record_non_useful_cache_bytes, 700);
    EXPECT_EQ(comparison.soa_non_useful_cache_bytes, 60);
    EXPECT_EQ(comparison.record_non_useful_page_bytes, 3'196);
    EXPECT_EQ(comparison.soa_non_useful_page_bytes, 7'292);
    EXPECT_EQ(comparison.useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.cache_line_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 10);
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 640);
    EXPECT_EQ(comparison.page_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.page_delta->magnitude, 1);
    EXPECT_EQ(comparison.page_byte_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.non_useful_cache_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.non_useful_cache_byte_delta->magnitude, 640);
    EXPECT_EQ(comparison.non_useful_page_byte_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(comparison.non_useful_page_byte_delta->magnitude, 4'096);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, RejectsIncompatibleRecordAndSoaAccessSets) {
    RecordAccessAnalysis record;
    record.member_names = {"first"};
    record.element_count = 10;
    SoaAccessAnalysis soa;
    soa.column_names = {"second"};
    soa.element_count = 10;

    auto comparison{Analyzer::compare_record_soa_access(record, soa)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());

    soa.column_names = {"first"};
    soa.element_count = 11;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    record.element_count = 11;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.record_non_useful_cache_bytes.has_value());
    EXPECT_FALSE(comparison.soa_non_useful_cache_bytes.has_value());
    EXPECT_FALSE(comparison.non_useful_cache_byte_delta.has_value());
    EXPECT_FALSE(comparison.record_non_useful_page_bytes.has_value());
    EXPECT_FALSE(comparison.soa_non_useful_page_bytes.has_value());
    EXPECT_FALSE(comparison.non_useful_page_byte_delta.has_value());

    record.accesses = {{.name = "first", .operation = AccessOperation::read}};
    soa.accesses = {{.name = "first", .operation = AccessOperation::write}};
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.write_useful_byte_delta.has_value());

    soa.accesses.front().operation = AccessOperation::read;
    soa.multiplicity = 2;
    comparison = Analyzer::compare_record_soa_access(record, soa);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.logical_write_useful_byte_delta.has_value());
}

TEST(SoaAnalyzer, ComparesSelectedAccessAcrossPhysicalVariants) {
    auto const fixture{soa_type(
        {{"small", "std::uint8_t"}, {"medium", "std::uint32_t"}, {"wide", "std::uint64_t"}})};
    auto abi{AbiProfile::host_common()};
    abi.set_memory_facts({.cache_line_bytes = 64,
                          .page_bytes = 4'096,
                          .l1_data_cache_bytes = 500,
                          .l2_cache_bytes = 448,
                          .l3_cache_bytes = 832,
                          .provenance = "test profile"});
    auto variant{Variant{}};
    variant.overrides.capacities[fixture.type] = 500;
    variant.overrides.soa_column_types[{.type = fixture.type, .field_name = "wide"}] =
        "std::uint32_t";
    auto const first{Analyzer::analyze_soa(fixture.types, fixture.type, Variant{}, abi, 1'000)};
    auto const second{Analyzer::analyze_soa(fixture.types, fixture.type, variant, abi, 1'000)};
    std::vector<std::string> const columns{"wide"};
    auto const first_access{
        Analyzer::analyze_soa_access(first, columns, abi, 100, AccessOperation::write, 5)};
    auto const second_access{
        Analyzer::analyze_soa_access(second, columns, abi, 100, AccessOperation::write, 5)};

    auto const comparison{Analyzer::compare_soa_access(first_access, second_access)};

    EXPECT_EQ(comparison.column_names, columns);
    ASSERT_EQ(comparison.columns.size(), 1);
    EXPECT_EQ(comparison.columns[0].name, "wide");
    EXPECT_EQ(comparison.columns[0].first.physical_type, "std::uint64_t");
    EXPECT_EQ(comparison.columns[0].second.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].first.element_bytes, 8);
    EXPECT_EQ(comparison.columns[0].second.element_bytes, 4);
    EXPECT_EQ(comparison.columns[0].element_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[0].useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.columns[0].cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.columns[0].cache_byte_delta->magnitude, 384);
    EXPECT_EQ(comparison.columns[0].first.non_payload_cache_bytes, 32);
    EXPECT_EQ(comparison.columns[0].second.non_payload_cache_bytes, 48);
    EXPECT_EQ(comparison.columns[0].non_payload_cache_byte_delta->magnitude, 16);
    EXPECT_EQ(comparison.columns[0].page_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.columns[0].first.non_payload_page_bytes, 3'296);
    EXPECT_EQ(comparison.columns[0].second.non_payload_page_bytes, 3'696);
    EXPECT_EQ(comparison.columns[0].non_payload_page_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.columns[0].allocated_capacity_payload_delta->magnitude, 6'000);
    EXPECT_EQ(comparison.columns[0].capacity_slack_payload_delta->magnitude, 5'600);
    EXPECT_EQ(comparison.element_count, 100);
    ASSERT_EQ(comparison.accesses.size(), 1);
    EXPECT_EQ(comparison.accesses.front().operation, AccessOperation::write);
    EXPECT_EQ(comparison.multiplicity, 5);
    EXPECT_EQ(comparison.first.useful_bytes, 800);
    EXPECT_EQ(comparison.second.useful_bytes, 400);
    EXPECT_EQ(comparison.first.read_useful_bytes, 0);
    EXPECT_EQ(comparison.second.read_useful_bytes, 0);
    EXPECT_EQ(comparison.first.write_useful_bytes, 800);
    EXPECT_EQ(comparison.second.write_useful_bytes, 400);
    EXPECT_EQ(comparison.first.logical_read_useful_bytes, 0);
    EXPECT_EQ(comparison.second.logical_read_useful_bytes, 0);
    EXPECT_EQ(comparison.first.logical_write_useful_bytes, 4'000);
    EXPECT_EQ(comparison.second.logical_write_useful_bytes, 2'000);
    EXPECT_EQ(comparison.first.read_cache_bytes, 0);
    EXPECT_EQ(comparison.second.read_cache_bytes, 0);
    EXPECT_EQ(comparison.first.write_cache_bytes, 832);
    EXPECT_EQ(comparison.second.write_cache_bytes, 448);
    EXPECT_EQ(comparison.read_useful_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.write_useful_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.write_useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.logical_read_useful_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->direction,
              NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.logical_write_useful_byte_delta->magnitude, 2'000);
    EXPECT_EQ(comparison.first_full_logical_payload_bytes, 1'300);
    EXPECT_EQ(comparison.second_full_logical_payload_bytes, 900);
    EXPECT_EQ(comparison.first_unselected_payload_bytes, 500);
    EXPECT_EQ(comparison.second_unselected_payload_bytes, 500);
    EXPECT_EQ(comparison.first.cache_lines, 13);
    EXPECT_EQ(comparison.second.cache_lines, 7);
    EXPECT_EQ(comparison.first.cache_bytes, 832);
    EXPECT_EQ(comparison.second.cache_bytes, 448);
    EXPECT_EQ(comparison.first.pages, 1);
    EXPECT_EQ(comparison.second.pages, 1);
    EXPECT_EQ(comparison.first_allocated_capacity_payload_bytes, 13'000);
    EXPECT_EQ(comparison.second_allocated_capacity_payload_bytes, 4'500);
    EXPECT_EQ(comparison.first_capacity_slack_payload_bytes, 11'700);
    EXPECT_EQ(comparison.second_capacity_slack_payload_bytes, 3'600);
    EXPECT_EQ(comparison.useful_byte_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(comparison.useful_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.cache_line_delta->magnitude, 6);
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 384);
    EXPECT_EQ(comparison.first_non_payload_cache_bytes, 32);
    EXPECT_EQ(comparison.second_non_payload_cache_bytes, 48);
    EXPECT_EQ(comparison.non_payload_cache_byte_delta->magnitude, 16);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.working_set_bytes, 832);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.working_set_bytes, 448);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l1_data, false);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l2, false);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l2, true);
    EXPECT_EQ(comparison.first_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.second_minimum_cache_footprint_capacity.fits_l3, true);
    EXPECT_EQ(comparison.page_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first_non_payload_page_bytes, 3'296);
    EXPECT_EQ(comparison.second_non_payload_page_bytes, 3'696);
    EXPECT_EQ(comparison.non_payload_page_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.full_logical_payload_delta->magnitude, 400);
    EXPECT_EQ(comparison.unselected_payload_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.allocated_capacity_payload_delta->magnitude, 8'500);
    EXPECT_EQ(comparison.capacity_slack_payload_delta->magnitude, 8'100);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, ComparesSelectedAccessAcrossTargetProfiles) {
    auto const fixture{soa_type({{"small", "std::uint8_t"}, {"wide", "std::uint32_t"}})};
    auto const first_profile{AbiProfile::host_common()};
    auto second_profile{first_profile};
    second_profile.set("std::uint32_t",
                       {.size_bytes = 8,
                        .alignment_bytes = 8,
                        .integer_signed = false,
                        .unsigned_value_bits = 32,
                        .provenance = {}});
    auto const first_layout{Analyzer::analyze_soa(fixture.types,
                                                  fixture.type,
                                                  Variant{},
                                                  first_profile,
                                                  100,
                                                  SoaAllocationStrategy::separate_columns)};
    auto const second_layout{Analyzer::analyze_soa(fixture.types,
                                                   fixture.type,
                                                   Variant{},
                                                   second_profile,
                                                   100,
                                                   SoaAllocationStrategy::separate_columns)};
    std::array const accesses{AccessIntent{.name = "wide", .operation = AccessOperation::write}};
    auto const first_access{
        Analyzer::analyze_soa_access(first_layout, accesses, first_profile, 10, 3)};
    auto const second_access{
        Analyzer::analyze_soa_access(second_layout, accesses, second_profile, 10, 3)};

    auto const comparison{Analyzer::compare_soa_access(first_access, second_access)};

    ASSERT_EQ(comparison.columns.size(), 1);
    EXPECT_EQ(comparison.columns[0].first.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].second.physical_type, "std::uint32_t");
    EXPECT_EQ(comparison.columns[0].element_byte_delta->magnitude, 4);
    EXPECT_EQ(comparison.columns[0].useful_byte_delta->magnitude, 40);
    EXPECT_EQ(comparison.columns[0].logical_write_useful_byte_delta->magnitude, 120);
    EXPECT_EQ(comparison.columns[0].cache_line_delta->magnitude, 1);
    EXPECT_EQ(comparison.first_total_allocation_bytes, 500);
    EXPECT_EQ(comparison.second_total_allocation_bytes, 900);
    EXPECT_EQ(comparison.total_allocation_byte_delta->magnitude, 400);
    EXPECT_EQ(comparison.first_alignment_padding_bytes, 0);
    EXPECT_EQ(comparison.second_alignment_padding_bytes, 0);
    EXPECT_EQ(comparison.alignment_padding_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.first.useful_bytes, 40);
    EXPECT_EQ(comparison.second.useful_bytes, 80);
    EXPECT_EQ(comparison.first.write_cache_bytes, 64);
    EXPECT_EQ(comparison.second.write_cache_bytes, 128);
    EXPECT_EQ(comparison.write_cache_byte_delta->magnitude, 64);
    EXPECT_EQ(comparison.first_allocated_capacity_payload_bytes, 500);
    EXPECT_EQ(comparison.second_allocated_capacity_payload_bytes, 900);
    EXPECT_EQ(comparison.allocated_capacity_payload_delta->magnitude, 400);
    EXPECT_EQ(comparison.first_capacity_slack_payload_bytes, 450);
    EXPECT_EQ(comparison.second_capacity_slack_payload_bytes, 810);
    EXPECT_EQ(comparison.capacity_slack_payload_delta->magnitude, 360);
    EXPECT_TRUE(comparison.diagnostics.empty());
}

TEST(SoaAnalyzer, RejectsIncompatibleSelectedAccessComparisons) {
    SoaAccessAnalysis first;
    first.column_names = {"first"};
    first.element_count = 10;
    auto second{first};
    second.column_names = {"second"};

    auto comparison{Analyzer::compare_soa_access(first, second)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());

    second.column_names = first.column_names;
    second.element_count = 11;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    second.element_count = first.element_count;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.columns.empty());
    EXPECT_FALSE(comparison.useful_byte_delta.has_value());

    second.columns = first.columns;
    first.accesses = {{.name = "first", .operation = AccessOperation::read}};
    second.accesses = {{.name = "first", .operation = AccessOperation::write}};
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.write_useful_byte_delta.has_value());

    second.accesses.front().operation = AccessOperation::read;
    second.allocation_strategy = SoaAllocationStrategy::aligned_contiguous;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.total_allocation_byte_delta.has_value());

    second.allocation_strategy = SoaAllocationStrategy::separate_columns;
    second.multiplicity = 2;
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_byte_delta.has_value());
    EXPECT_FALSE(comparison.logical_write_useful_byte_delta.has_value());

    first.column_names = {"first", "second"};
    second = first;
    SoaColumnAccessAnalysis first_column;
    first_column.name = "first";
    SoaColumnAccessAnalysis second_column;
    second_column.name = "second";
    first.columns = {first_column, second_column};
    second.columns = {first_column};
    comparison = Analyzer::compare_soa_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.columns.empty());
}

} // namespace
} // namespace ioj::layout
