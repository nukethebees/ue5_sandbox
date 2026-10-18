#include "analyzer_test_fixtures.hpp"

#include <cmath>

namespace ioj::layout {
namespace analyzer_tests {

class RepresentationTest : public testing::TestWithParam<AnalysisCase> {};

TEST_P(RepresentationTest, ReportsSemanticAndEncodingFacts) {
    auto const& test{GetParam()};
    auto const document{Document::from_source(test.source)};
    ASSERT_TRUE(document.valid());
    test.verify(document);
}

AnalysisCase const cases[]{
    {"EnumImplicitAndSentinels",
     R"((module game :header "Game.h"
        (enum Value std::uint8_t :count Count
          (value Zero) (value One) (value Invalid :value "15" :sentinel true)
          (value Count :value "16"))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_enum(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_EQ(result.live_value_count, 2);
         EXPECT_EQ(result.reserved_value_count, 2);
         EXPECT_EQ(result.minimum_required_bits, 5);
         EXPECT_EQ(result.backing_bits, 8);
         EXPECT_TRUE(result.enumerators[2].sentinel);
         EXPECT_TRUE(result.enumerators[3].count_sentinel);
     }},
    {"SignedEnum",
     R"((module game :header "Game.h"
        (enum Value std::int8_t (value Low :value "-4") (value High :value "3"))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_enum(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_EQ(result.signed_domain, true);
         EXPECT_EQ(result.minimum_required_bits, 3);
         EXPECT_EQ(result.backing_can_represent_domain, true);
         EXPECT_EQ(result.unused_backing_codes, 254);
     }},
    {"EnumExpressionStaysUnknown",
     R"((module game :header "Game.h"
        (enum Value std::uint8_t (value Mask :value "1 << 3") (value Next))))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_enum(
             doc.types(), doc.find_type("game", "Value").value(), AbiProfile::host_common())};
         EXPECT_FALSE(result.enumerators[0].code);
         EXPECT_FALSE(result.enumerators[1].code);
         EXPECT_FALSE(result.minimum_required_bits);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"ScalarDomainAndSentinel",
     R"((module game :header "Game.h"
        (integer-scalar Value :signed false :minimum 0 :maximum 10
          (code Invalid :value 15 :sentinel true))))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_integer_scalar(doc.types(), doc.find_type("game", "Value").value())};
         EXPECT_EQ(result.live_value_count, 11);
         EXPECT_EQ(result.sentinel_code_count, 1);
         EXPECT_EQ(result.minimum_required_bits, 4);
         EXPECT_EQ(result.effective_bit_width, 4);
         EXPECT_EQ(result.unused_codes, 4);
     }},
    {"FullUnsignedDomain",
     R"((module game :header "Game.h"
        (integer-scalar Value :signed false :minimum 0 :maximum 18446744073709551615)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_integer_scalar(doc.types(), doc.find_type("game", "Value").value())};
         EXPECT_EQ(result.effective_bit_width, 64);
         EXPECT_EQ(result.unused_codes, 0);
         EXPECT_FALSE(result.live_value_count);
     }},
    {"OptionalPolicies",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 10
          (code Invalid :value 15 :sentinel true))
        (optional-sentinel Sentinel :source Domain :sentinel Invalid)
        (optional-presence-bit Presence :source Domain)))",
     [](Document const& doc) {
         auto const sentinel{doc.find_type("game", "Sentinel").value()};
         auto const presence{doc.find_type("game", "Presence").value()};
         auto const first{Analyzer::analyze_optional_sentinel(doc.types(), sentinel, 100)};
         auto const second{Analyzer::analyze_optional_presence_bit(doc.types(), presence, 100)};
         EXPECT_EQ(first.encoded_storage_bits, 4);
         EXPECT_EQ(first.total_encoded_bits, 400);
         EXPECT_EQ(second.encoded_storage_bits, 5);
         EXPECT_EQ(second.total_encoded_bits, 500);
         EXPECT_EQ(second.noncanonical_absence_patterns, 15);
         auto const comparison{
             Analyzer::compare_optional_encodings(doc.types(), sentinel, presence, 100)};
         ASSERT_TRUE(comparison.compatible_source);
         EXPECT_EQ(comparison.total_encoded_bit_delta->magnitude, 100);
     }},
    {"Optional65BitBoundary",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 18446744073709551615)
        (optional-presence-bit Value :source Domain)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_optional_presence_bit(doc.types(),
                                                     doc.find_type("game", "Value").value(),
                                                     std::numeric_limits<std::uint64_t>::max())};
         EXPECT_EQ(result.encoded_storage_bits, 65);
         EXPECT_FALSE(result.total_encoded_bits);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"QuantizationPrecision",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -100 :maximum 100)
        (linear-quantized Coarse :source Domain :bits 8 :reserved-codes 2 :clipping clamp)
        (linear-quantized Fine :source Domain :bits 16 :reserved-codes 0 :clipping reject)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::compare_linear_quantized(doc.types(),
                                                doc.find_type("game", "Coarse").value(),
                                                doc.find_type("game", "Fine").value(),
                                                10)};
         EXPECT_TRUE(result.compatible_source);
         EXPECT_EQ(result.first.usable_code_count.value, 254);
         EXPECT_NEAR(static_cast<double>(result.first.resolution), 200.0 / 253, 1e-12);
         EXPECT_TRUE(result.first.minimum_endpoint_exact);
         EXPECT_TRUE(result.first.maximum_endpoint_exact);
         EXPECT_LT(result.second.resolution, result.first.resolution);
         EXPECT_EQ(result.total_encoded_bit_delta->magnitude, 80);
         EXPECT_EQ(result.clipping_changed, true);
     }},
    {"Quantization64BitCapacity",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 1000)
        (linear-quantized Value :source Domain :bits 64 :reserved-codes 0 :clipping reject)))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_linear_quantized(
             doc.types(), doc.find_type("game", "Value").value())};
         EXPECT_TRUE(result.total_code_count.two_to_64);
         EXPECT_TRUE(result.usable_code_count.two_to_64);
         EXPECT_GT(result.resolution, 0);
     }},
    {"FixedPoint",
     R"((module game :header "Game.h"
        (fixed-point Value :signed true :total-bits 12 :fractional-bits 4 :rounding toward-zero)))",
     [](Document const& doc) {
         auto const result{Analyzer::analyze_fixed_point(
             doc.types(), doc.find_type("game", "Value").value(), 10)};
         EXPECT_EQ(result.whole_bits, 7);
         EXPECT_EQ(result.resolution, 0.0625L);
         EXPECT_EQ(result.minimum_value, -128.0L);
         EXPECT_EQ(result.maximum_value, 127.9375L);
         EXPECT_EQ(result.total_encoded_bits, 120);
     }},
    {"MiniFloat",
     R"((module game :header "Game.h"
        (mini-float Value :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 15)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_mini_float(doc.types(), doc.find_type("game", "Value").value(), 10)};
         EXPECT_EQ(result.total_bits, 16);
         EXPECT_EQ(result.zero_code_count, 2);
         EXPECT_EQ(result.infinity_code_count, 2);
         EXPECT_EQ(result.nan_code_count, 2046);
         EXPECT_EQ(result.maximum_finite, 65504.0L);
         EXPECT_EQ(result.minimum_positive_normal, std::ldexp(1.0L, -14));
         EXPECT_EQ(result.total_encoded_bits, 160);
     }},
    {"MiniFloatNumericOverflow",
     R"((module game :header "Game.h"
        (mini-float Value :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias -32768)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_mini_float(doc.types(), doc.find_type("game", "Value").value())};
         EXPECT_EQ(result.minimum_normal_exponent, 32769);
         EXPECT_FALSE(result.maximum_finite);
         EXPECT_FALSE(result.minimum_positive_normal);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"VarintDistribution",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 16384)
        (integer-varint Value :source Domain :encoding unsigned)))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         auto const result{Analyzer::analyze_integer_varint(doc.types(), type, 100)};
         EXPECT_EQ(result.minimum_total_bytes, 100);
         EXPECT_EQ(result.maximum_total_bytes, 300);
         std::array const entries{IntegerVarintDistributionEntry{.value = 0, .weight = 1},
                                  IntegerVarintDistributionEntry{.value = 128, .weight = 3},
                                  IntegerVarintDistributionEntry{.value = 16384, .weight = 1}};
         auto const distribution{
             Analyzer::analyze_integer_varint_distribution(doc.types(), type, entries, 100)};
         EXPECT_EQ(distribution.total_weight, 5);
         EXPECT_EQ(distribution.total_encoded_bytes, 10);
         EXPECT_EQ(distribution.expected_bytes_per_value, 2.0L);
         EXPECT_EQ(distribution.expected_selected_bytes, 200.0L);
         auto const empty{Analyzer::analyze_integer_varint_distribution(doc.types(), type, {})};
         EXPECT_FALSE(empty.expected_bytes_per_value);
         EXPECT_FALSE(empty.diagnostics.empty());
     }},
    {"SignedVarintComparison",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -65 :maximum 64)
        (integer-varint Signed :source Domain :encoding signed)
        (integer-varint Zigzag :source Domain :encoding zigzag)))",
     [](Document const& doc) {
         auto const result{Analyzer::compare_integer_varint(doc.types(),
                                                            doc.find_type("game", "Signed").value(),
                                                            doc.find_type("game", "Zigzag").value(),
                                                            100)};
         EXPECT_TRUE(result.compatible_source);
         EXPECT_EQ(result.first.minimum_total_bytes, 100);
         EXPECT_EQ(result.second.maximum_total_bytes, 200);
         EXPECT_EQ(result.maximum_encoded_byte_delta->magnitude, 0);
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
     [](Document const& doc) {
         auto const optional{
             Analyzer::compare_optional_encodings(doc.types(),
                                                  doc.find_type("game", "FirstOptional").value(),
                                                  doc.find_type("game", "SecondOptional").value())};
         EXPECT_FALSE(optional.compatible_source);
         EXPECT_FALSE(optional.encoded_storage_bit_delta);
         EXPECT_FALSE(optional.diagnostics.empty());
         auto const quantized{
             Analyzer::compare_linear_quantized(doc.types(),
                                                doc.find_type("game", "FirstQuantized").value(),
                                                doc.find_type("game", "SecondQuantized").value())};
         EXPECT_FALSE(quantized.compatible_source);
         EXPECT_FALSE(quantized.diagnostics.empty());
         auto const varint{
             Analyzer::compare_integer_varint(doc.types(),
                                              doc.find_type("game", "FirstVarint").value(),
                                              doc.find_type("game", "SecondVarint").value())};
         EXPECT_FALSE(varint.compatible_source);
         EXPECT_FALSE(varint.minimum_encoded_byte_delta);
         EXPECT_FALSE(varint.diagnostics.empty());
     }},
    {"VarintInvalidAndOverflowedWeights",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed false :minimum 0 :maximum 128)
        (integer-varint Value :source Domain :encoding unsigned)))",
     [](Document const& doc) {
         auto const type{doc.find_type("game", "Value").value()};
         std::array const invalid{IntegerVarintDistributionEntry{.value = 129, .weight = 1}};
         auto const rejected{
             Analyzer::analyze_integer_varint_distribution(doc.types(), type, invalid)};
         EXPECT_EQ(rejected.valid_entry_count, 0);
         EXPECT_FALSE(rejected.expected_bytes_per_value);
         EXPECT_FALSE(rejected.diagnostics.empty());
         std::array const overflow{IntegerVarintDistributionEntry{
             .value = 128, .weight = std::numeric_limits<std::uint64_t>::max()}};
         auto const result{
             Analyzer::analyze_integer_varint_distribution(doc.types(), type, overflow)};
         EXPECT_FALSE(result.total_encoded_bytes);
         EXPECT_EQ(result.expected_bytes_per_value, 2.0L);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"FullWidthFixedPointOverflow",
     R"((module game :header "Game.h"
        (fixed-point Value :signed true :total-bits 64 :fractional-bits 0 :rounding nearest-even)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_fixed_point(doc.types(),
                                           doc.find_type("game", "Value").value(),
                                           std::numeric_limits<std::uint64_t>::max())};
         EXPECT_EQ(result.total_bits, 64);
         EXPECT_EQ(result.minimum_raw_value,
                   lispb::codegen::PackedIntegerValue{std::numeric_limits<std::int64_t>::min()});
         EXPECT_FALSE(result.total_encoded_bits);
         EXPECT_FALSE(result.diagnostics.empty());
     }},
    {"VarintAggregateOverflow",
     R"((module game :header "Game.h"
        (integer-scalar Domain :signed true :minimum -9223372036854775808 :maximum 9223372036854775807)
        (integer-varint Value :source Domain :encoding zigzag)))",
     [](Document const& doc) {
         auto const result{
             Analyzer::analyze_integer_varint(doc.types(),
                                              doc.find_type("game", "Value").value(),
                                              std::numeric_limits<std::uint64_t>::max())};
         EXPECT_EQ(result.maximum_encoded_bytes, 10);
         EXPECT_FALSE(result.maximum_total_bytes);
         EXPECT_FALSE(result.diagnostics.empty());
     }}};

INSTANTIATE_TEST_SUITE_P(Source,
                         RepresentationTest,
                         testing::ValuesIn(cases),
                         [](testing::TestParamInfo<AnalysisCase> const& test) {
                             return std::string{test.param.name};
                         });

}
}
