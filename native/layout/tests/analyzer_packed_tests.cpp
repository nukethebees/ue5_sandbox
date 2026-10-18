#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/resolved_variant.hpp>

namespace ioj::layout {
namespace analyzer_packed_tests {

class PackedAnalysisTest : public testing::TestWithParam<AnalysisCase> {};

TEST_P(PackedAnalysisTest, ReportsPhysicalAndSemanticFacts) {
    EditorSession editor;
    auto const& test{GetParam()};
    auto const document{editor.from_source(test.source)};
    ASSERT_TRUE(document.valid());
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    test.verify(context);
}

AnalysisCase const cases[]{
    {"ReservedAndUnused",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field index std::uint32_t :bits 20)
          (reserved future :bits 4) (field kind std::uint8_t :bits 4))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 100})};
         auto const& result{context.storage.types.packed[run.analysis.value].value()};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(result.storage_bits, 32);
         EXPECT_EQ(result.payload_bits, 24);
         EXPECT_EQ(result.reserved_bits, 4);
         EXPECT_EQ(result.unused_bits, 4);
         ASSERT_EQ(result.fields.count, 3);
         EXPECT_EQ(fields.most_significant_bit[result.fields.begin], 19);
         EXPECT_EQ(fields.maximum_unsigned_value[result.fields.begin], 1048575);
         EXPECT_TRUE(fields.reserved[result.fields.begin + 1]);
         EXPECT_EQ(fields.least_significant_bit[result.fields.begin + 2], 24);
         EXPECT_EQ(context.storage.totals[0].total_storage_bytes, 400);
         EXPECT_EQ(context.storage.totals[0].total_reserved_bits, 400);
         EXPECT_EQ(context.storage.totals[0].total_unused_bits, 400);
         EXPECT_FALSE(has_analysis_diagnostics(context));
     }},
    {"MostSignificantFirst",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t :byte-order big :bit-order msb-first
          (field index std::uint32_t :bits 20) (reserved future :bits 4)
          (field kind std::uint8_t :bits 8))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const& result{context.storage.types.packed[run.analysis.value].value()};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(result.byte_order, lispb::codegen::PackedByteOrder::big_endian);
         EXPECT_EQ(fields.least_significant_bit[result.fields.begin], 12);
         EXPECT_EQ(fields.most_significant_bit[result.fields.begin], 31);
         EXPECT_EQ(fields.least_significant_bit[result.fields.begin + 1], 8);
         EXPECT_EQ(fields.least_significant_bit[result.fields.begin + 2], 0);
     }},
    {"SignedDomain",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field delta std::int32_t :bits auto :kind signed :minimum -100 :maximum 100
            (code Invalid :value -128 :sentinel true)))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const field{context.storage.types.packed[run.analysis.value]->fields.begin};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(fields.bit_width[field], 8);
         EXPECT_EQ(fields.minimum_signed_value[field], -128);
         EXPECT_EQ(fields.maximum_signed_value[field], 127);
         EXPECT_EQ(fields.semantic_value_count[field], 201);
         EXPECT_EQ(fields.sentinel_code_count[field], 1);
         EXPECT_EQ(fields.unused_codes[field], 54);
     }},
    {"RepresentedFields",
     R"((module game :header "Game.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 1000)
        (linear-quantized Quantized :source Health :bits 8 :reserved-codes 2 :clipping clamp)
        (fixed-point Velocity :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero)
        (packed-value Value :storage std::uint32_t
          (field health Quantized :bits auto :kind linear-quantized)
          (field speed Velocity :bits auto :kind fixed-point))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const& result{context.storage.types.packed[run.analysis.value].value()};
         EXPECT_EQ(result.payload_bits, 20);
         auto const& scalars{context.storage.scalars};
         auto const health{
             scalars.find(context.storage.fields.semantic_type[result.fields.begin].value())};
         auto const speed{
             scalars.find(context.storage.fields.semantic_type[result.fields.begin + 1].value())};
         ASSERT_TRUE(scalars.quantized_encoding[health]);
         EXPECT_EQ(scalars.quantized_encoding[health]->reserved_code_count, 2);
         ASSERT_TRUE(scalars.fixed_encoding[speed]);
         EXPECT_EQ(scalars.fixed_encoding[speed]->resolution, 0.0625L);
     }},
    {"TargetComparison",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t (field value std::uint32_t :bits 24))))",
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
             AnalysisRequest{.type = type, .abi = &abi, .element_count = 100},
             AnalysisRequest{.type = type, .abi = &wide, .element_count = 100},
             AnalysisRequest{.type = type, .abi = &unknown, .element_count = 100},
             AnalysisRequest{.type = type,
                             .abi = &abi,
                             .element_count = std::numeric_limits<std::uint64_t>::max()}};
         Analyzer::analyze(context, requests);
         auto const& storage{context.storage};
         auto const& first{storage.types.packed[storage.runs[0].analysis.value].value()};
         auto const& second{storage.types.packed[storage.runs[1].analysis.value].value()};
         EXPECT_EQ(second.storage_facts->size_bytes - first.storage_facts->size_bytes, 4);
         EXPECT_EQ(*storage.totals[1].total_storage_bytes - *storage.totals[0].total_storage_bytes,
                   400);
         EXPECT_EQ(*second.unused_bits - *first.unused_bits, 32);
         auto const& missing{storage.types.packed[storage.runs[2].analysis.value].value()};
         EXPECT_FALSE(missing.storage_bits);
         EXPECT_FALSE(missing.storage_facts);
         EXPECT_FALSE(storage.totals[3].total_storage_bytes);
         EXPECT_TRUE(has_analysis_diagnostics(context));
     }},
    {"SelectedAccess",
     R"((module game :header "Game.h"
        (packed-value Value :storage std::uint32_t
          (field index std::uint32_t :bits 20) (reserved future :bits 4)
          (field kind std::uint8_t :bits 8))))",
     [](AnalysisContext& context) {
         AccessSet const accesses{{"kind", AccessOperation::read}};
         auto const run{run_analysis(
             context, "Value", {.element_count = 100, .multiplicity = 3, .accesses = &accesses})};
         auto const row{run.access->value};
         auto const& result{context.storage.workloads};
         EXPECT_EQ(result.bits[row].useful, 800);
         EXPECT_EQ(result.bits[row].logical_read, 2400);
         EXPECT_EQ(result.storage_bytes[row], 400);
         EXPECT_EQ(result.packed_waste[row].unselected_field_bits, 2000);
         EXPECT_EQ(result.packed_waste[row].reserved_bits, 400);
         EXPECT_EQ(result.cache[row].count, 7);
         AccessSet const invalid{{"future", AccessOperation::read}};
         run_analysis(context, "Value", {.element_count = 100, .accesses = &invalid});
         EXPECT_TRUE(has_analysis_diagnostics(context));
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
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    run_analysis(context, "Handle", {.default_capacity = test.extent});
    ASSERT_EQ(storage.relationships.metrics.size(), 1);
    auto const& metrics{storage.relationships.metrics[0].value()};
    EXPECT_EQ(metrics.minimum_required_bits, test.expected_bits);
    EXPECT_EQ(metrics.width_sufficient, test.expected_bits <= 8);
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
