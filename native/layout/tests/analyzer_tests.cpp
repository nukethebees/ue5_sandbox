#include "analyzer_test_fixtures.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/compare_runs.hpp>
#include <ioj/layout/editor_session.hpp>

#include <cmath>

namespace ioj::layout::analyzer_tests {

class RepresentationTest : public AnalysisTest {};

TEST_P(RepresentationTest, ReportsSemanticAndEncodingFacts) {
    verify_case();
}

AnalysisCase const cases[]{
    {"EnumImplicitAndSentinels",
     R"((module game :header "Game.h"
        (enum Value std::uint8_t :count Count
          (value Zero) (value One) (value Invalid :value "15" :sentinel true)
          (value Count :value "16"))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 1})};
         auto const& result{scalar_result(context, context.storage.scalars.enum_domain())};
         auto const& backing{context.storage.types.enum_backing()[run.analysis.value].value()};
         EXPECT_EQ(result.live_value_count, 2);
         EXPECT_EQ(result.reserved_value_count, 2);
         EXPECT_EQ(result.minimum_required_bits, 5);
         EXPECT_EQ(backing.backing_bits, 8);
         EXPECT_TRUE(context.storage.codes.sentinel()[result.codes.begin + 2]);
         EXPECT_TRUE(context.storage.codes.count_sentinel()[result.codes.begin + 3]);
     }},
    {"SignedEnum",
     R"((module game :header "Game.h"
        (enum Value std::int8_t (value Low :value "-4") (value High :value "3"))))",
     [](AnalysisContext& context) {
         auto const run{run_analysis(context, "Value", {.element_count = 1})};
         auto const& result{scalar_result(context, context.storage.scalars.enum_domain())};
         auto const& backing{context.storage.types.enum_backing()[run.analysis.value].value()};
         EXPECT_EQ(result.signed_domain, true);
         EXPECT_EQ(result.minimum_required_bits, 3);
         EXPECT_EQ(backing.backing_can_represent_domain, true);
         EXPECT_EQ(backing.unused_backing_codes, 254);
     }},
    {"EnumExpressionStaysUnknown",
     R"((module game :header "Game.h"
        (enum Value std::uint8_t (value Mask :value "1 << 3") (value Next))))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {}, true);
         auto const& result{scalar_result(context, context.storage.scalars.enum_domain())};
         EXPECT_FALSE(context.storage.codes.code()[result.codes.begin + 0]);
         EXPECT_FALSE(context.storage.codes.code()[result.codes.begin + 1]);
         EXPECT_FALSE(result.minimum_required_bits);
     }},
    {"ScalarDomainAndSentinel",
     R"((module game :header "Game.h"
        (integer-scalar Value :signed false :minimum 0 :maximum 10
          (code Invalid :value 15 :sentinel true))))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 1});
         auto const& result{scalar_result(context, context.storage.scalars.integer_domain())};
         EXPECT_EQ(result,
                   (IntegerDomain{.maximum_value = 10,
                                  .live_value_count = 11,
                                  .sentinel_code_count = 1,
                                  .required_code_count = 12,
                                  .minimum_required_bits = 4,
                                  .effective_bit_width = 4,
                                  .unused_codes = 4,
                                  .codes = {result.codes.begin, 1}}));
     }},
    {"FullUnsignedDomain",
     R"((module game :header "Game.h"
        (integer-scalar Value :signed false :minimum 0 :maximum 18446744073709551615)))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 1});
         auto const& result{scalar_result(context, context.storage.scalars.integer_domain())};
         EXPECT_EQ(result,
                   (IntegerDomain{.maximum_value = std::numeric_limits<std::uint64_t>::max(),
                                  .minimum_required_bits = 64,
                                  .effective_bit_width = 64,
                                  .unused_codes = 0}));
     }},
    {"OptionalPolicies",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 10
          (code Invalid :value 15 :sentinel true))
        (optional-sentinel Sentinel :source Domain :sentinel Invalid)
        (optional-presence-bit Presence :source Domain)))",
     [](AnalysisContext& context) {
         auto const& doc{context.document};
         auto const sentinel{doc.find_type("game", "Sentinel").value()};
         auto const presence{doc.find_type("game", "Presence").value()};
         auto const abi{AbiProfile::host_common()};
         std::array const requests{
             AnalysisRequest{.type = sentinel, .abi = &abi, .element_count = 100},
             AnalysisRequest{.type = presence, .abi = &abi, .element_count = 100}};
         analyze_requests(context, requests);
         auto const& storage{context.storage};
         auto const& first{
             scalar_result(context, storage.scalars.optional_sentinel_encoding(), "Sentinel")};
         auto const& second{
             scalar_result(context, storage.scalars.optional_presence_encoding(), "Presence")};
         EXPECT_EQ(first.encoded_storage_bits, 4);
         EXPECT_EQ(storage.totals[0].total_encoded_bits, 400);
         EXPECT_EQ(second.encoded_storage_bits, 5);
         EXPECT_EQ(storage.totals[1].total_encoded_bits, 500);
         EXPECT_EQ(second.noncanonical_absence_patterns, 15);
         ASSERT_TRUE(compare_runs(context, 0, 1));
         EXPECT_EQ(*storage.totals[1].total_encoded_bits - *storage.totals[0].total_encoded_bits,
                   100);
     }},
    {"Optional65BitBoundary",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 18446744073709551615)
        (optional-presence-bit Value :source Domain)))",
     [](AnalysisContext& context) {
         run_analysis(
             context, "Value", {.element_count = std::numeric_limits<std::uint64_t>::max()}, true);
         auto const& result{
             scalar_result(context, context.storage.scalars.optional_presence_encoding())};
         EXPECT_EQ(result.encoded_storage_bits, 65);
         EXPECT_FALSE(context.storage.totals[0].total_encoded_bits);
     }},
    {"QuantizationPrecision",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -100 :maximum 100)
        (linear-quantized Coarse :source Domain :bits 8 :reserved-codes 2 :clipping clamp)
        (linear-quantized Fine :source Domain :bits 16 :reserved-codes 0 :clipping reject)))",
     [](AnalysisContext& context) {
         auto const& doc{context.document};
         auto const coarse{doc.find_type("game", "Coarse").value()};
         auto const fine{doc.find_type("game", "Fine").value()};
         auto const abi{AbiProfile::host_common()};
         std::array const requests{
             AnalysisRequest{.type = coarse, .abi = &abi, .element_count = 10},
             AnalysisRequest{.type = fine, .abi = &abi, .element_count = 10}};
         analyze_requests(context, requests);
         auto const& storage{context.storage};
         auto const& first{scalar_result(context, storage.scalars.quantized_encoding(), "Coarse")};
         auto const& second{scalar_result(context, storage.scalars.quantized_encoding(), "Fine")};
         EXPECT_TRUE(compare_runs(context, 0, 1));
         EXPECT_EQ(first.usable_code_count.value, 254);
         EXPECT_NEAR(static_cast<double>(first.resolution), 200.0 / 253, 1e-12);
         EXPECT_TRUE(first.minimum_endpoint_exact);
         EXPECT_TRUE(first.maximum_endpoint_exact);
         EXPECT_LT(second.resolution, first.resolution);
         EXPECT_EQ(*storage.totals[1].total_encoded_bits - *storage.totals[0].total_encoded_bits,
                   80);
         EXPECT_NE(first.clipping, second.clipping);
     }},
    {"Quantization64BitCapacity",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 1000)
        (linear-quantized Value :source Domain :bits 64 :reserved-codes 0 :clipping reject)))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 1});
         auto const& result{scalar_result(context, context.storage.scalars.quantized_encoding())};
         EXPECT_TRUE(result.total_code_count.two_to_64);
         EXPECT_TRUE(result.usable_code_count.two_to_64);
         EXPECT_GT(result.resolution, 0);
     }},
    {"FixedPoint",
     R"((module game :header "Game.h"
        (fixed-point Value :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero)))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 10});
         auto const& result{scalar_result(context, context.storage.scalars.fixed_encoding())};
         EXPECT_EQ(result.whole_bits, 7);
         EXPECT_EQ(result.resolution, 0.0625L);
         EXPECT_EQ(result.minimum_value, -128.0L);
         EXPECT_EQ(result.maximum_value, 127.9375L);
         EXPECT_EQ(context.storage.totals[0].total_encoded_bits, 120);
     }},
    {"MiniFloat",
     R"((module game :header "Game.h"
        (mini-float Value :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 15)))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 10});
         auto const& result{scalar_result(context, context.storage.scalars.float_encoding())};
         EXPECT_EQ(result.total_bits, 16);
         EXPECT_EQ(result.zero_code_count, 2);
         EXPECT_EQ(result.infinity_code_count, 2);
         EXPECT_EQ(result.nan_code_count, 2046);
         EXPECT_EQ(result.maximum_finite, 65504.0L);
         EXPECT_EQ(result.minimum_positive_normal, std::ldexp(1.0L, -14));
         EXPECT_EQ(context.storage.totals[0].total_encoded_bits, 160);
     }},
    {"MiniFloatNumericOverflow",
     R"((module game :header "Game.h"
        (mini-float Value :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias -32768)))",
     [](AnalysisContext& context) {
         run_analysis(context, "Value", {.element_count = 1}, true);
         auto const& result{scalar_result(context, context.storage.scalars.float_encoding())};
         EXPECT_EQ(result.minimum_normal_exponent, 32769);
         EXPECT_FALSE(result.maximum_finite);
         EXPECT_FALSE(result.minimum_positive_normal);
     }},
    {"VarintDistribution",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 16384)
        (integer-varint Value :source Domain :encoding unsigned)))",
     [](AnalysisContext& context) {
         std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1},
                                  IntegerVarintDistributionEntry{.value = 128, .weight = 3},
                                  IntegerVarintDistributionEntry{.value = 16384, .weight = 1}};
         auto const run{run_analysis(
             context, "Value", {.element_count = 100, .varint_distribution = entries})};
         EXPECT_EQ(context.storage.totals[0].minimum_total_bytes, 100);
         EXPECT_EQ(context.storage.totals[0].maximum_total_bytes, 300);
         auto const distribution{run.distribution->value};
         auto const& values{context.storage.distributions};
         EXPECT_EQ(values.total_weight()[distribution], 5);
         EXPECT_EQ(values.total_extent_bytes()[distribution], 10);
         EXPECT_EQ(values.expected_extent_bytes()[distribution], 2.0L);
         EXPECT_EQ(values.expected_selected_extent_bytes()[distribution], 200.0L);
         auto const empty{run_analysis(context, "Value", {.distribution_requested = true}, true)};
         EXPECT_FALSE(
             context.storage.distributions.expected_extent_bytes()[empty.distribution->value]);
     }},
    {"SignedVarintComparison",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -65 :maximum 64)
        (integer-varint Signed :source Domain :encoding signed)
        (integer-varint Zigzag :source Domain :encoding zigzag)))",
     [](AnalysisContext& context) {
         auto const& doc{context.document};
         auto const first{doc.find_type("game", "Signed").value()};
         auto const second{doc.find_type("game", "Zigzag").value()};
         auto const abi{AbiProfile::host_common()};
         std::array const requests{
             AnalysisRequest{.type = first, .abi = &abi, .element_count = 100},
             AnalysisRequest{.type = second, .abi = &abi, .element_count = 100}};
         analyze_requests(context, requests);
         EXPECT_TRUE(compare_runs(context, 0, 1));
         EXPECT_EQ(context.storage.totals[0].minimum_total_bytes, 100);
         EXPECT_EQ(context.storage.totals[1].maximum_total_bytes, 200);
         auto const& scalars{context.storage.scalars};
         EXPECT_EQ(
             scalar_result(context, scalars.varint_encoding(), "Signed").maximum_encoded_bytes,
             scalar_result(context, scalars.varint_encoding(), "Zigzag").maximum_encoded_bytes);
     }},
    {"IncompatibleSources",
     R"((module game :header "Game.h"
        (integer-scalar First :signed false :minimum 0 :maximum 100)
        (integer-scalar Second :signed false :minimum 0 :maximum 100)
        (optional-presence-bit FirstOptional :source First)
        (optional-presence-bit SecondOptional :source Second)
        (linear-quantized FirstQuantized :source First :bits 8 :reserved-codes 0 :clipping reject)
        (linear-quantized SecondQuantized :source Second :bits 8 :reserved-codes 0 :clipping reject)
        (integer-varint FirstVarint :source First :encoding unsigned)
        (integer-varint SecondVarint :source Second :encoding unsigned)))",
     [](AnalysisContext& context) {
         auto const abi{AbiProfile::host_common()};
         for (auto const suffix : {"Optional", "Quantized", "Varint"}) {
             SCOPED_TRACE(suffix);
             auto const first{
                 context.document.find_type("game", std::string{"First"} + suffix).value()};
             auto const second{
                 context.document.find_type("game", std::string{"Second"} + suffix).value()};
             std::array const requests{AnalysisRequest{.type = first, .abi = &abi},
                                       AnalysisRequest{.type = second, .abi = &abi}};
             analyze_requests(context, requests);
             EXPECT_FALSE(compare_runs(context, 0, 1));
             expect_analysis_diagnostics(context);
         }
     }},
    {"VarintInvalidAndOverflowedWeights",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 128)
        (integer-varint Value :source Domain :encoding unsigned)))",
     [](AnalysisContext& context) {
         std::array const invalid{IntegerVarintDistributionEntry{.value = 129, .weight = 1}};
         auto const rejected{
             run_analysis(context, "Value", {.varint_distribution = invalid}, true)};
         auto const row{rejected.distribution->value};
         EXPECT_EQ(context.storage.distributions.entries()[row].count, 0);
         EXPECT_FALSE(context.storage.distributions.expected_extent_bytes()[row]);
         std::array const overflow{IntegerVarintDistributionEntry{
             .value = 128, .weight = std::numeric_limits<std::uint64_t>::max()}};
         auto const run{run_analysis(context, "Value", {.varint_distribution = overflow}, true)};
         EXPECT_FALSE(context.storage.distributions.total_extent_bytes()[run.distribution->value]);
         EXPECT_EQ(context.storage.distributions.expected_extent_bytes()[run.distribution->value],
                   2.0L);
     }},
    {"FullWidthFixedPointOverflow",
     R"((module game :header "Game.h"
        (fixed-point Value :signed true :total-bits 64 :fractional-bits 0 :rounding nearest-even)))",
     [](AnalysisContext& context) {
         run_analysis(
             context, "Value", {.element_count = std::numeric_limits<std::uint64_t>::max()}, true);
         auto const& result{scalar_result(context, context.storage.scalars.fixed_encoding())};
         EXPECT_EQ(result.total_bits, 64);
         EXPECT_EQ(result.minimum_raw_value,
                   lispb::codegen::PackedIntegerValue{std::numeric_limits<std::int64_t>::min()});
         EXPECT_FALSE(context.storage.totals[0].total_encoded_bits);
     }},
    {"VarintAggregateOverflow",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -9223372036854775808 :maximum 9223372036854775807)
        (integer-varint Value :source Domain :encoding zigzag)))",
     [](AnalysisContext& context) {
         run_analysis(
             context, "Value", {.element_count = std::numeric_limits<std::uint64_t>::max()}, true);
         auto const& result{scalar_result(context, context.storage.scalars.varint_encoding())};
         EXPECT_EQ(result.maximum_encoded_bytes, 10);
         EXPECT_FALSE(context.storage.totals[0].maximum_total_bytes);
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         RepresentationTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

} // namespace ioj::layout::analyzer_tests
