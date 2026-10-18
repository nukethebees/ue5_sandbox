#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/editor_session.hpp>

namespace ioj::layout {
namespace analyzer_packed_tests {

class PackedAnalysisTest : public testing::TestWithParam<AnalysisCase> {};

TEST_P(PackedAnalysisTest, ReportsPhysicalAndSemanticFacts) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const document{editor.from_source(test.source)};
    ASSERT_TRUE(document.valid());
    test.verify(document);
}

AnalysisCase const cases[]{
    {"ReservedAndUnused",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field index std::uint32_t :bits 20)
          (reserved future :bits 4) (field kind std::uint8_t :bits 4))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_packed(doc.types(),
                                                    doc.find_type("game", "Value").value(),
                                                    ResolvedVariant{},
                                                    AbiProfile::host_common(),
                                                    100)};
         EXPECT_EQ(result.storage_bits, 32);
         EXPECT_EQ(result.payload_bits, 24);
         EXPECT_EQ(result.reserved_bits, 4);
         EXPECT_EQ(result.unused_bits, 4);
         ASSERT_EQ(result.fields.size(), 3);
         EXPECT_EQ(result.fields[0].most_significant_bit, 19);
         EXPECT_EQ(result.fields[0].maximum_unsigned_value, 1048575);
         EXPECT_TRUE(result.fields[1].reserved);
         EXPECT_EQ(result.fields[2].least_significant_bit, 24);
         EXPECT_EQ(result.aggregate.total_storage_bytes, 400);
         EXPECT_EQ(result.aggregate.total_reserved_bits, 400);
         EXPECT_EQ(result.aggregate.total_unused_bits, 400);
         EXPECT_TRUE(result.diagnostics.empty());
     }},
    {"MostSignificantFirst",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t :byte-order big :bit-order msb-first
          (field index std::uint32_t :bits 20) (reserved future :bits 4)
          (field kind std::uint8_t :bits 8))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_packed(doc.types(),
                                                    doc.find_type("game", "Value").value(),
                                                    ResolvedVariant{},
                                                    AbiProfile::host_common())};
         EXPECT_EQ(result.byte_order, lispb::codegen::PackedByteOrder::big_endian);
         EXPECT_EQ(result.fields[0].least_significant_bit, 12);
         EXPECT_EQ(result.fields[0].most_significant_bit, 31);
         EXPECT_EQ(result.fields[1].least_significant_bit, 8);
         EXPECT_EQ(result.fields[2].least_significant_bit, 0);
     }},
    {"SignedDomain",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field delta std::int32_t :bits auto :kind signed :minimum -100 :maximum 100
            (code Invalid :value -128 :sentinel true)))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_packed(doc.types(),
                                                    doc.find_type("game", "Value").value(),
                                                    ResolvedVariant{},
                                                    AbiProfile::host_common())};
         auto const& field{result.fields.front()};
         EXPECT_EQ(field.bit_width, 8);
         EXPECT_EQ(field.minimum_signed_value, -128);
         EXPECT_EQ(field.maximum_signed_value, 127);
         EXPECT_EQ(field.semantic_value_count, 201);
         EXPECT_EQ(field.sentinel_code_count, 1);
         EXPECT_EQ(field.unused_codes, 54);
     }},
    {"RepresentedFields",
     R"((module game :header "Game.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 1000)
        (linear-quantized Quantized :source Health :bits 8 :reserved-codes 2 :clipping clamp)
        (fixed-point Velocity :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero)
        (packed-value Value :storage std::uint32_t
          (field health Quantized :bits auto :kind linear-quantized)
          (field speed Velocity :bits auto :kind fixed-point))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_packed(doc.types(),
                                                    doc.find_type("game", "Value").value(),
                                                    ResolvedVariant{},
                                                    AbiProfile::host_common())};
         EXPECT_EQ(result.payload_bits, 20);
         ASSERT_TRUE(result.fields[0].linear_quantized);
         EXPECT_EQ(result.fields[0].linear_quantized->reserved_code_count, 2);
         ASSERT_TRUE(result.fields[1].fixed_point);
         EXPECT_EQ(result.fields[1].fixed_point->resolution, 0.0625L);
     }},
    {"TargetComparison",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t (field value std::uint32_t :bits 24))))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         auto wide{AbiProfile::host_common()};
         wide.set("std::uint32_t",
                  {.size_bytes = 8,
                   .alignment_bytes = 8,
                   .integer_signed = false,
                   .unsigned_value_bits = 32,
                   .provenance = {}});
         auto const first{Analyzer::analyze_packed(
             doc.types(), type, ResolvedVariant{}, AbiProfile::host_common(), 100)};
         auto const second{
             Analyzer::analyze_packed(doc.types(), type, ResolvedVariant{}, wide, 100)};
         auto const comparison{Analyzer::compare_packed_targets(first, second)};
         EXPECT_EQ(comparison.storage_size_delta->magnitude, 4);
         EXPECT_EQ(comparison.total_storage_delta->magnitude, 400);
         EXPECT_EQ(comparison.unused_bit_delta->magnitude, 32);
         auto const unknown{Analyzer::analyze_packed(
             doc.types(), type, ResolvedVariant{}, AbiProfile{"unknown"}, 100)};
         EXPECT_FALSE(unknown.storage_bits);
         EXPECT_FALSE(Analyzer::compare_packed_targets(first, unknown).storage_size_delta);
         auto const overflow{Analyzer::analyze_packed(doc.types(),
                                                      type,
                                                      ResolvedVariant{},
                                                      AbiProfile::host_common(),
                                                      std::numeric_limits<std::uint64_t>::max())};
         EXPECT_FALSE(overflow.aggregate.total_storage_bytes);
         EXPECT_FALSE(overflow.diagnostics.empty());
     }},
    {"SelectedAccess",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field index std::uint32_t :bits 20) (reserved future :bits 4)
          (field kind std::uint8_t :bits 8))))",
     [](Document const& doc) {
         auto const abi{AbiProfile::host_common()};
         auto const packed{Analyzer::analyze_packed(
             doc.types(), doc.find_type("game", "Value").value(), ResolvedVariant{}, abi, 100)};
         std::array const accesses{AccessIntent{"kind", AccessOperation::read}};
         auto const result{Analyzer::analyze_packed_access(packed, accesses, abi, 3)};
         EXPECT_EQ(result.useful_bits, 800);
         EXPECT_EQ(result.logical_read_useful_bits, 2400);
         EXPECT_EQ(result.storage_footprint_bytes, 400);
         EXPECT_EQ(result.unselected_field_bits, 2000);
         EXPECT_EQ(result.reserved_region_bits, 400);
         EXPECT_EQ(result.minimum_cache_lines_touched, 7);
         std::array const invalid{AccessIntent{"future", AccessOperation::read}};
         EXPECT_FALSE(Analyzer::analyze_packed_access(packed, invalid, abi).diagnostics.empty());
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         PackedAnalysisTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

struct CapacityCase {
    std::string_view name;
    std::string_view relation;
    std::uint64_t extent;
    std::uint32_t expected_bits;
};

class RelationshipCapacityTest : public testing::TestWithParam<CapacityCase> {};

TEST_P(RelationshipCapacityTest, DerivesWidthFromTargetExtent) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const source{std::string{R"((module game :header "Game.h" :backend standard-library
        (struct Entities (member values array std::uint8_t))
        (packed-value Handle :storage std::uint64_t
          (field index std::uint64_t :bits 8 (relation )"} +
                      std::string{test.relation} + " Entities))))"};
    auto const document{editor.from_source(source)};
    ASSERT_TRUE(document.valid());
    std::array const targets{RelationshipTargetFacts{
        document.find_type("game", "Entities").value(), test.extent, test.extent}};
    auto const result{Analyzer::analyze_packed(document.types(),
                                               document.find_type("game", "Handle").value(),
                                               ResolvedVariant{},
                                               AbiProfile::host_common(),
                                               1,
                                               targets)};
    auto const& field{result.fields.front()};
    EXPECT_EQ(field.relationship_minimum_required_bits, test.expected_bits);
    EXPECT_EQ(field.relationship_width_sufficient, test.expected_bits <= 8);
}

INSTANTIATE_TEST_SUITE_P(Extents,
                         RelationshipCapacityTest,
                         testing::Values(CapacityCase{"IndexFits", "index_into", 256, 8},
                                         CapacityCase{"IndexGrows", "index_into", 257, 9},
                                         CapacityCase{"CountIncludesEndpoint", "count_of", 256, 9}),
                         [](testing::TestParamInfo<CapacityCase> const& test) {
                             return std::string{test.param.name};
                         });

}
}
