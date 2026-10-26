#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/editor_session.hpp>
#include <ioj/layout/resolved_variant.hpp>
#include <ioj/layout/transaction.hpp>
#include <ioj/layout/variants.hpp>
#include <ioj/single_allocation/layout.h>

namespace ioj::layout::analyzer_packed_tests {

static_assert(single_allocation_layout::supported_leaf<lispb::StringId>);
static_assert(single_allocation_layout::supported_leaf<std::optional<PhysicalFactId>>);
static_assert(single_allocation_layout::supported_leaf<std::optional<EnumBacking>>);
static_assert(single_allocation_layout::supported_leaf<std::optional<PackedLayout>>);
static_assert(single_allocation_layout::supported_leaf<std::optional<AggregateLayout>>);
static_assert(single_allocation_layout::supported_leaf<std::optional<OptionalSentinelEncoding>>);

TEST(AnalysisStrings, RetainsInternedTextAcrossGrowthAndReanalysis) {
    EditorSession editor;
    auto const document{editor.from_source(R"((module game :header "Game.h"
        (record Value :layout packed :bits 32
          (member first std::uint8_t :bits 8)
          (member second std::uint8_t :bits 8)
    (projections (cpp :storage std::uint32_t)))))")};
    ASSERT_TRUE(document.valid());
    AnalysisStorage storage;
    AnalysisContext context{document, storage};

    auto const run{run_analysis(context)};
    auto const result{storage.types.packed()[run.analysis.value].value()};
    auto const name{storage.fields.logical_type()[result.fields.begin]};
    EXPECT_EQ(name, storage.fields.logical_type()[result.fields.begin + 1]);
    EXPECT_EQ(storage.strings.view(name), "std::uint8_t");
    EXPECT_EQ(storage.strings.view(result.storage_type), "std::uint32_t");
    EXPECT_EQ(result.storage_type, result.schema_storage_type);

    static_cast<void>(storage.strings.intern(std::string(16384, 'x')));
    EXPECT_EQ(storage.strings.view(name), "std::uint8_t");
    EXPECT_EQ(storage.strings.view(result.storage_type), "std::uint32_t");

    storage.clear();
    EXPECT_EQ(storage.strings.byte_size(), 0);
    EXPECT_EQ(storage.fields.num(), 0);
    auto const next{run_analysis(context)};
    EXPECT_EQ(storage.strings.view(storage.types.packed()[next.analysis.value]->storage_type),
              "std::uint32_t");
}

class PackedAnalysisTest : public AnalysisTest {};

TEST(PackedAnalysis, UsesResolvedNestedPlacementsAndDeduplicatesOverlappingAccesses) {
    EditorSession editor;
    auto const document{editor.from_source(R"((module game :header "Game.h"
        (integer-scalar Code :bit-width 4)
        (record Inner (member code @Code))
        (union Choices (member first @Inner) (member second @Inner))
        (record Value :layout packed :bits 16
          (member present bool)
          (member payload @Choices :bits 8)
          (projections (cpp)))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    AccessSet const accesses{{"payload", AccessOperation::read},
                             {"payload_first_code", AccessOperation::read}};
    auto const run{run_analysis(context, "Value", {.element_count = 10, .accesses = &accesses})};
    auto const& result{storage.types.packed()[run.analysis.value].value()};
    auto const type{document.find_type("game", "Value").value()};
    auto const& resolved{document.types().bit_layout(type)};
    ASSERT_EQ(result.fields.count, resolved.placements.size());
    for (std::size_t index{}; index < resolved.placements.size(); ++index) {
        auto const row{result.fields.begin + index};
        EXPECT_EQ(storage.fields.name()[row], resolved.placements[index].path);
        EXPECT_EQ(storage.fields.least_significant_bit()[row], resolved.placements[index].offset);
        EXPECT_EQ(storage.fields.bit_width()[row], resolved.placements[index].extent);
    }
    EXPECT_EQ(storage.workloads.bits()[run.access->value].useful, 80);
    EXPECT_EQ(storage.workloads.packed_waste()[run.access->value].unselected_field_bits, 10);
    EXPECT_EQ(result.unused_bits, 7);
}

TEST(PackedAnalysis, ResolvesWiderAllocationAndStorageTogether) {
    EditorSession editor;
    auto document{editor.from_source(R"((module game :header "Game.h"
        (integer-scalar Code :bit-width 4)
        (record Value :layout packed
          (member code @Code :bits 8)
          (projections (cpp :storage std::uint8_t)))))")};
    ASSERT_TRUE(document.valid()) << diagnostic_messages(editor);
    auto create{document.transaction()};
    create.create_variant("Wider");
    ASSERT_TRUE(document.commit(create));
    auto const variant{document.variants().ids().back()};
    auto edit{document.transaction()};
    edit.set_variant_field_bits(variant, "game", "Value", "code", 16);
    edit.set_variant_packed_storage(variant, "game", "Value", "std::uint32_t");
    ASSERT_TRUE(document.commit(edit));
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    auto const run{run_analysis(context, "Value", {.variant = variant})};
    auto const& result{storage.types.packed()[run.analysis.value].value()};
    EXPECT_EQ(result.storage_bits, 32);
    EXPECT_EQ(result.bits_used, 16);
    EXPECT_EQ(storage.fields.bit_width()[result.fields.begin], 16);
    EXPECT_EQ(document.types().bit_layout(document.find_type("game", "Value").value()).encoded_bits,
              8);
}

TEST_P(PackedAnalysisTest, ReportsPhysicalAndSemanticFacts) {
    verify_case();
}

AnalysisCase const cases[]{
    {"ReservedAndUnused",
     R"((module game :header "Game.h"
        (integer-scalar Value_indexEncoding :signed false :bit-width 20)
        (integer-scalar Value_kindEncoding :signed false :bit-width 4)
        (record Value :layout packed :bits 32
          (member index @Value_indexEncoding)
          (reserved future :bits 4) (member kind @Value_kindEncoding)
    (projections (cpp :storage std::uint32_t)))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 100})};
         auto const& result{context.storage.types.packed()[run.analysis.value].value()};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(result.storage_bits, 32);
         EXPECT_EQ(result.payload_bits, 24);
         EXPECT_EQ(result.reserved_bits, 4);
         EXPECT_EQ(result.unused_bits, 4);
         ASSERT_EQ(result.fields.count, 3);
         EXPECT_EQ(fields.most_significant_bit()[result.fields.begin], 19);
         EXPECT_EQ(fields.maximum_unsigned_value()[result.fields.begin], 1048575);
         EXPECT_TRUE(fields.reserved()[result.fields.begin + 1]);
         EXPECT_EQ(fields.least_significant_bit()[result.fields.begin + 2], 24);
         EXPECT_EQ(context.storage.totals[0].total_storage_bytes, 400);
         EXPECT_EQ(context.storage.totals[0].total_reserved_bits, 400);
         EXPECT_EQ(context.storage.totals[0].total_unused_bits, 400);
     }},
    {"MostSignificantFirst",
     R"((module game :header "Game.h"
        (integer-scalar Value_indexEncoding :signed false :bit-width 20)
        (record Value :layout packed :bits 32 :byte-order big :bit-order msb-first
          (member index @Value_indexEncoding) (reserved future :bits 4)
          (member kind std::uint8_t :bits 8)
    (projections (cpp :storage std::uint32_t)))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const& result{context.storage.types.packed()[run.analysis.value].value()};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(result.byte_order, lispb::codegen::PackedByteOrder::big_endian);
         EXPECT_EQ(fields.least_significant_bit()[result.fields.begin], 12);
         EXPECT_EQ(fields.most_significant_bit()[result.fields.begin], 31);
         EXPECT_EQ(fields.least_significant_bit()[result.fields.begin + 1], 8);
         EXPECT_EQ(fields.least_significant_bit()[result.fields.begin + 2], 0);
     }},
    {"SignedDomain",
     R"((module game :header "Game.h"
        (integer-scalar Value_deltaEncoding :signed true :minimum -100 :maximum 100 (code Invalid :value -128 :sentinel true))
        (record Value :layout packed :bits 32
          (member delta @Value_deltaEncoding
            )
    (projections (cpp :storage std::uint32_t)))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const field{context.storage.types.packed()[run.analysis.value]->fields.begin};
         auto const& fields{context.storage.fields};
         EXPECT_EQ(fields.bit_width()[field], 8);
         EXPECT_EQ(fields.minimum_signed_value()[field], -128);
         EXPECT_EQ(fields.maximum_signed_value()[field], 127);
         EXPECT_EQ(fields.semantic_value_count()[field], 201);
         EXPECT_EQ(fields.sentinel_code_count()[field], 1);
         EXPECT_EQ(fields.unused_codes()[field], 54);
     }},
    {"RepresentedFields",
     R"((module game :header "Game.h"
        (integer-scalar Health :signed false :minimum 0 :maximum 1000)
        (linear-quantized Quantized :source Health :bits 8 :reserved-codes 2 :clipping clamp)
        (fixed-point Velocity :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero)
        (record Value :layout packed :bits 32
          (member health @Quantized)
          (member speed @Velocity)
    (projections (cpp :storage std::uint32_t)))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context)};
         auto const& result{context.storage.types.packed()[run.analysis.value].value()};
         EXPECT_EQ(result.payload_bits, 20);
         auto const& scalars{context.storage.scalars};
         auto const health{
             scalars.find(context.storage.fields.semantic_type()[result.fields.begin].value())};
         auto const speed{
             scalars.find(context.storage.fields.semantic_type()[result.fields.begin + 1].value())};
         ASSERT_TRUE(scalars.quantized_encoding()[health]);
         EXPECT_EQ(scalars.quantized_encoding()[health]->reserved_code_count, 2);
         ASSERT_TRUE(scalars.fixed_encoding()[speed]);
         EXPECT_EQ(scalars.fixed_encoding()[speed]->resolution, 0.0625L);
     }},
    {"TargetComparison",
     R"((module game :header "Game.h"
        (integer-scalar Value_valueEncoding :signed false :bit-width 24)
        (record Value :layout packed :bits 32 (member value @Value_valueEncoding)
    (projections (cpp :storage std::uint32_t)))))",
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
         analyze_requests(context, requests, true);
         auto const& storage{context.storage};
         auto const& first{storage.types.packed()[storage.runs[0].analysis.value].value()};
         auto const& second{storage.types.packed()[storage.runs[1].analysis.value].value()};
         EXPECT_EQ(context.storage.physical_facts.size_bytes()[second.storage_facts->value] -
                       context.storage.physical_facts.size_bytes()[first.storage_facts->value],
                   4);
         EXPECT_EQ(*storage.totals[1].total_storage_bytes - *storage.totals[0].total_storage_bytes,
                   400);
         EXPECT_EQ(*second.unused_bits - *first.unused_bits, 32);
         auto const& missing{storage.types.packed()[storage.runs[2].analysis.value].value()};
         EXPECT_FALSE(missing.storage_bits);
         EXPECT_FALSE(missing.storage_facts);
         EXPECT_FALSE(storage.totals[3].total_storage_bytes);
     }},
    {"SelectedAccess",
     R"((module game :header "Game.h"
        (integer-scalar Value_indexEncoding :signed false :bit-width 20)
        (record Value :layout packed :bits 32
          (member index @Value_indexEncoding) (reserved future :bits 4)
          (member kind std::uint8_t :bits 8)
    (projections (cpp :storage std::uint32_t)))))",
     [](AnalysisContext& context) {
         AccessSet const accesses{{"kind", AccessOperation::read}};
         auto const run{run_analysis(
             context, "Value", {.element_count = 100, .multiplicity = 3, .accesses = &accesses})};
         auto const row{run.access->value};
         auto const& result{context.storage.workloads};
         EXPECT_EQ(result.bits()[row],
                   (PayloadFootprint{.useful = 800,
                                     .read = 800,
                                     .write = 0,
                                     .logical_read = 2400,
                                     .logical_write = 0}));
         EXPECT_EQ(result.storage_bytes()[row], 400);
         EXPECT_EQ(result.packed_waste()[row].unselected_field_bits, 2000);
         EXPECT_EQ(result.packed_waste()[row].reserved_bits, 400);
         EXPECT_EQ(result.cache()[row].count, 7);
         AccessSet const invalid{{"future", AccessOperation::read}};
         run_analysis(context, "Value", {.element_count = 100, .accesses = &invalid}, true);
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
    auto const source{std::string{R"((module game :header "Game.h"
        (struct Entities (member values array std::uint8_t))
        (integer-scalar Index :bit-width 8)
        (record Handle :layout packed :bits 64
          (projections (cpp :storage std::uint64_t))
          (member index @Index (relation )"} +
                      std::string{test.relation} + " Entities))))"};
    auto const document{editor.from_source(source)};
    ASSERT_TRUE(document.valid());
    AnalysisStorage storage;
    AnalysisContext context{document, storage};
    run_analysis(context, "Handle", {.default_capacity = test.extent}, test.expected_bits > 8);
    ASSERT_EQ(storage.relationships.num(), 1);
    auto const& metrics{storage.relationships.metrics()[0].value()};
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

} // namespace ioj::layout::analyzer_packed_tests
