#include "analyzer_test_fixtures.hpp"

namespace ioj::layout {
namespace {

TEST(PackedAnalyzer, ReportsEntityUniqueIdLayout) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.storage_facts->size_bytes, 4);
    EXPECT_EQ(analysis.storage_bits, 32);
    EXPECT_EQ(analysis.bits_used, 32);
    EXPECT_EQ(analysis.payload_bits, 32);
    EXPECT_EQ(analysis.reserved_bits, 0);
    EXPECT_EQ(analysis.unused_bits, 0);
    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_EQ(analysis.fields[0].least_significant_bit, 0);
    EXPECT_EQ(analysis.fields[0].most_significant_bit, 23);
    EXPECT_EQ(analysis.fields[0].maximum_unsigned_value, 16'777'215);
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 24);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 31);
    EXPECT_EQ(analysis.fields[1].maximum_unsigned_value, 255);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ComparesPhysicalLayoutAcrossTargetProfiles) {
    auto const fixture{entity_id_type(
        false, false, std::nullopt, codegen::PackedBitOrder::most_significant_first)};
    auto first_target{AbiProfile::host_common()};
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "synthetic wide target"});
    auto const first{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, first_target, 100)};
    auto const second{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, second_target, 100)};

    auto const comparison{Analyzer::compare_packed_targets(first, second)};

    ASSERT_EQ(comparison.fields.size(), 2U);
    EXPECT_EQ(comparison.first.storage_bits, 32U);
    EXPECT_EQ(comparison.second.storage_bits, 64U);
    ASSERT_TRUE(comparison.storage_size_delta.has_value());
    EXPECT_EQ(comparison.storage_size_delta->magnitude, 4U);
    ASSERT_TRUE(comparison.storage_alignment_delta.has_value());
    EXPECT_EQ(comparison.storage_alignment_delta->magnitude, 4U);
    ASSERT_TRUE(comparison.storage_bit_delta.has_value());
    EXPECT_EQ(comparison.storage_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.unused_bit_delta.has_value());
    EXPECT_EQ(comparison.unused_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.total_storage_delta.has_value());
    EXPECT_EQ(comparison.total_storage_delta->magnitude, 400U);
    ASSERT_TRUE(comparison.total_unused_bit_delta.has_value());
    EXPECT_EQ(comparison.total_unused_bit_delta->magnitude, 3'200U);
    ASSERT_TRUE(comparison.minimum_cache_line_delta.has_value());
    EXPECT_EQ(comparison.minimum_cache_line_delta->magnitude, 6U);
    ASSERT_TRUE(comparison.fields[0].least_significant_bit_delta.has_value());
    EXPECT_EQ(comparison.fields[0].least_significant_bit_delta->magnitude, 32U);
    ASSERT_TRUE(comparison.fields[1].most_significant_bit_delta.has_value());
    EXPECT_EQ(comparison.fields[1].most_significant_bit_delta->magnitude, 32U);
    EXPECT_TRUE(comparison.diagnostics.empty());

    auto const identical{Analyzer::compare_packed_targets(first, first)};
    ASSERT_TRUE(identical.storage_bit_delta.has_value());
    EXPECT_EQ(identical.storage_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(identical.storage_bit_delta->magnitude, 0U);
}

TEST(PackedAnalyzer, PreservesUnknownAndOverflowAcrossTargetComparison) {
    auto const fixture{entity_id_type()};
    auto const known{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};
    auto const unknown{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile{"unknown"}, 10)};
    auto unknown_comparison{Analyzer::compare_packed_targets(known, unknown)};

    ASSERT_EQ(unknown_comparison.fields.size(), 2U);
    EXPECT_FALSE(unknown_comparison.storage_size_delta.has_value());
    EXPECT_FALSE(unknown_comparison.storage_bit_delta.has_value());
    EXPECT_FALSE(unknown_comparison.minimum_cache_line_delta.has_value());
    EXPECT_TRUE(
        std::ranges::any_of(unknown_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target packed layout:");
        }));

    auto narrow_target{AbiProfile::host_common()};
    narrow_target.set("std::uint32_t",
                      {.size_bytes = 2,
                       .alignment_bytes = 2,
                       .integer_signed = false,
                       .unsigned_value_bits = 16,
                       .provenance = "synthetic narrow target"});
    auto const overflow{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, narrow_target, 10)};
    auto const overflow_comparison{Analyzer::compare_packed_targets(known, overflow)};

    EXPECT_EQ(overflow_comparison.second.overflow_bits, 16U);
    ASSERT_TRUE(overflow_comparison.overflow_bit_delta.has_value());
    EXPECT_EQ(overflow_comparison.overflow_bit_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(overflow_comparison.overflow_bit_delta->magnitude, 16U);
    EXPECT_TRUE(
        std::ranges::any_of(overflow_comparison.diagnostics, [](Diagnostic const& diagnostic) {
            return diagnostic.message.starts_with("Second target packed layout:") &&
                   diagnostic.severity == DiagnosticSeverity::error;
        }));
}

TEST(PackedAnalyzer, RejectsMismatchedTargetComparisonInputsWithoutPartialFields) {
    auto const fixture{entity_id_type()};
    auto const first{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 10)};
    auto second{first};
    second.fields[1].name = "different";

    auto comparison{Analyzer::compare_packed_targets(first, second)};

    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same ordered segments"),
              std::string::npos);

    second = first;
    second.aggregate.element_count = 11;
    comparison = Analyzer::compare_packed_targets(first, second);
    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("different element counts"),
              std::string::npos);

    second = first;
    second.storage_type = "std::uint64_t";
    comparison = Analyzer::compare_packed_targets(first, second);
    EXPECT_TRUE(comparison.fields.empty());
    ASSERT_FALSE(comparison.diagnostics.empty());
    EXPECT_NE(comparison.diagnostics.back().message.find("same physical variant"),
              std::string::npos);
}

TEST(PackedAnalyzer, SeparatesReservedPayloadAndTrailingUnusedBits) {
    auto const fixture{entity_id_type(true)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 100)};

    EXPECT_EQ(analysis.storage_bits, 32);
    EXPECT_EQ(analysis.bits_used, 32);
    EXPECT_EQ(analysis.payload_bits, 28);
    EXPECT_EQ(analysis.reserved_bits, 4);
    EXPECT_EQ(analysis.unused_bits, 0);
    ASSERT_EQ(analysis.fields.size(), 3U);
    EXPECT_FALSE(analysis.fields[0].reserved);
    EXPECT_TRUE(analysis.fields[1].reserved);
    EXPECT_FALSE(analysis.fields[1].semantic_type.has_value());
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 20U);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 23U);
    EXPECT_FALSE(analysis.fields[1].maximum_unsigned_value.has_value());
    EXPECT_EQ(analysis.fields[2].least_significant_bit, 24U);
    EXPECT_EQ(analysis.aggregate.total_payload_bits, 2'800U);
    EXPECT_EQ(analysis.aggregate.total_reserved_bits, 400U);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 0U);
}

TEST(PackedAnalyzer, ReportsExplicitByteOrderAndMostSignificantFirstRanges) {
    auto const fixture{entity_id_type(true,
                                      false,
                                      codegen::PackedByteOrder::big_endian,
                                      codegen::PackedBitOrder::most_significant_first)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    EXPECT_EQ(analysis.byte_order, codegen::PackedByteOrder::big_endian);
    EXPECT_EQ(analysis.bit_order, codegen::PackedBitOrder::most_significant_first);
    ASSERT_EQ(analysis.fields.size(), 3U);
    EXPECT_EQ(analysis.fields[0].least_significant_bit, 12U);
    EXPECT_EQ(analysis.fields[0].most_significant_bit, 31U);
    EXPECT_EQ(analysis.fields[1].least_significant_bit, 8U);
    EXPECT_EQ(analysis.fields[1].most_significant_bit, 11U);
    EXPECT_EQ(analysis.fields[2].least_significant_bit, 0U);
    EXPECT_EQ(analysis.fields[2].most_significant_bit, 7U);

    auto compact_fixture{entity_id_type(false,
                                        false,
                                        codegen::PackedByteOrder::little_endian,
                                        codegen::PackedBitOrder::most_significant_first)};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = compact_fixture.type, .field_name = "index"}] =
        20;
    auto compact{Analyzer::analyze_packed(
        compact_fixture.types, compact_fixture.type, variant, AbiProfile::host_common())};
    EXPECT_EQ(compact.unused_bits, 4U);
    EXPECT_EQ(compact.fields[0].least_significant_bit, 12U);
    EXPECT_EQ(compact.fields[0].most_significant_bit, 31U);
    EXPECT_EQ(compact.fields[1].least_significant_bit, 4U);
    EXPECT_EQ(compact.fields[1].most_significant_bit, 11U);

    variant.overrides.packed_field_widths[{.type = compact_fixture.type, .field_name = "index"}] =
        25;
    auto overflow{Analyzer::analyze_packed(
        compact_fixture.types, compact_fixture.type, variant, AbiProfile::host_common())};
    EXPECT_EQ(overflow.overflow_bits, 1U);
    EXPECT_EQ(overflow.fields[0].least_significant_bit, 7U);
    EXPECT_EQ(overflow.fields[0].most_significant_bit, 31U);
    EXPECT_FALSE(overflow.fields[1].most_significant_bit.has_value());
}

TEST(PackedAnalyzer, ReportsPackedFieldSemanticRangeCodeSpace) {
    auto const fixture{entity_id_type(true, true)};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    auto const& index{analysis.fields[0]};
    EXPECT_EQ(index.minimum_semantic_value, 0U);
    EXPECT_EQ(index.maximum_semantic_value, 1'000'000U);
    EXPECT_EQ(index.semantic_value_count, 1'000'001U);
    EXPECT_EQ(index.sentinel_code_count, 2U);
    EXPECT_EQ(index.required_code_count, 1'000'003U);
    EXPECT_EQ(index.minimum_required_bits, 20U);
    EXPECT_TRUE(index.schema_bit_width_auto);
    EXPECT_EQ(index.schema_bit_width, 20U);
    EXPECT_EQ(index.unused_codes, 48'573U);
    ASSERT_EQ(index.named_codes.size(), 3U);
    EXPECT_EQ(index.named_codes[1].name, "Invalid");
    EXPECT_TRUE(index.named_codes[1].sentinel);
    EXPECT_EQ(index.relationship_kind, codegen::SemanticRelationKind::index_into);
    EXPECT_EQ(index.relationship_target, "EntityType");

    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 19;
    auto const too_narrow{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};
    EXPECT_FALSE(too_narrow.fields[0].unused_codes.has_value());
    EXPECT_FALSE(too_narrow.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSharedIntegerScalarFieldDomain) {
    auto const fixture{packed_integer_scalar_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 1U);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.logical_type, "Health");
    EXPECT_EQ(field.schema_bit_width, 12U);
    EXPECT_TRUE(field.schema_bit_width_auto);
    EXPECT_EQ(field.minimum_semantic_value, 0U);
    EXPECT_EQ(field.maximum_semantic_value, 1000U);
    EXPECT_EQ(field.semantic_value_count, 1001U);
    EXPECT_EQ(field.sentinel_code_count, 1U);
    EXPECT_EQ(field.required_code_count, 1002U);
    EXPECT_EQ(field.minimum_required_bits, 12U);
    EXPECT_EQ(field.unused_codes, 3094U);
    ASSERT_EQ(field.named_codes.size(), 1U);
    EXPECT_EQ(field.named_codes.front().name, "Invalid");
    EXPECT_TRUE(field.named_codes.front().sentinel);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsPlacedLinearQuantizationFacts) {
    auto const fixture{packed_linear_quantized_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.kind, codegen::PackedFieldKind::linear_quantized);
    EXPECT_EQ(field.schema_bit_width, 8U);
    EXPECT_TRUE(field.schema_bit_width_auto);
    ASSERT_TRUE(field.linear_quantized.has_value());
    EXPECT_EQ(field.linear_quantized->source_minimum, 0U);
    EXPECT_EQ(field.linear_quantized->source_maximum, 1000U);
    EXPECT_EQ(field.linear_quantized->encoded_storage_bits, 8U);
    EXPECT_EQ(field.linear_quantized->usable_code_count,
              (ExactCodeCount{.value = 254, .two_to_64 = false}));
    EXPECT_EQ(field.linear_quantized->reserved_code_count, 2U);
    EXPECT_NEAR(static_cast<double>(field.linear_quantized->resolution), 1000.0 / 253.0, 1e-12);
    EXPECT_EQ(field.linear_quantized->clipping, codegen::QuantizationClipping::clamp);
    EXPECT_TRUE(analysis.diagnostics.empty());

    Variant overridden;
    overridden.overrides.packed_field_widths[{.type = fixture.type, .field_name = "health"}] = 7;
    auto const ignored_override{Analyzer::analyze_packed(
        fixture.types, fixture.type, overridden, AbiProfile::host_common())};
    EXPECT_EQ(ignored_override.fields.front().bit_width, 8U);
    EXPECT_FALSE(ignored_override.fields.front().overridden);
    ASSERT_EQ(ignored_override.diagnostics.size(), 1U);
    EXPECT_EQ(ignored_override.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(PackedAnalyzer, ReportsPlacedFixedPointFacts) {
    auto const fixture{packed_fixed_point_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.kind, codegen::PackedFieldKind::fixed_point);
    EXPECT_EQ(field.schema_bit_width, 12U);
    EXPECT_TRUE(field.schema_bit_width_auto);
    ASSERT_TRUE(field.fixed_point.has_value());
    EXPECT_TRUE(field.fixed_point->signedness);
    EXPECT_EQ(field.fixed_point->total_bits, 12U);
    EXPECT_EQ(field.fixed_point->whole_bits, 7U);
    EXPECT_EQ(field.fixed_point->fractional_bits, 4U);
    EXPECT_EQ(field.fixed_point->minimum_raw_value, codegen::PackedIntegerValue{-2048});
    EXPECT_EQ(field.fixed_point->maximum_raw_value, codegen::PackedIntegerValue{2047});
    EXPECT_EQ(field.fixed_point->minimum_value, -128.0L);
    EXPECT_EQ(field.fixed_point->maximum_value, 127.9375L);
    EXPECT_EQ(field.fixed_point->resolution, 0.0625L);
    EXPECT_EQ(field.fixed_point->rounding, codegen::FixedPointRounding::toward_zero);
    EXPECT_TRUE(analysis.diagnostics.empty());

    Variant overridden;
    overridden.overrides.packed_field_widths[{.type = fixture.type, .field_name = "velocity"}] = 11;
    auto const ignored_override{Analyzer::analyze_packed(
        fixture.types, fixture.type, overridden, AbiProfile::host_common())};
    EXPECT_EQ(ignored_override.fields.front().bit_width, 12U);
    EXPECT_FALSE(ignored_override.fields.front().overridden);
    ASSERT_EQ(ignored_override.diagnostics.size(), 1U);
    EXPECT_EQ(ignored_override.diagnostics.front().severity, DiagnosticSeverity::warning);
}

TEST(PackedAnalyzer, DerivesIndexCapacityWidthWithSentinel) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::index_into, 12, 4'095)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'000, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    ASSERT_EQ(fits.fields.size(), 1);
    auto const& field{fits.fields.front()};
    EXPECT_EQ(field.relationship_target_extent, 4'000);
    EXPECT_EQ(field.relationship_live_value_count,
              (ExactCodeCount{.value = 4'000, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_required_code_count,
              (ExactCodeCount{.value = 4'001, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_minimum_required_bits, 12);
    EXPECT_EQ(field.relationship_width_sufficient, true);
    EXPECT_EQ(field.relationship_code_space_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_capacity_headroom, 95);
    EXPECT_EQ(field.relationship_semantic_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_sentinel_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_effective_capacity_limit, 4'095);
    EXPECT_EQ(field.relationship_effective_capacity_headroom, 95);
    EXPECT_TRUE(fits.diagnostics.empty());

    auto overflow_capacity{capacity};
    overflow_capacity.front().element_capacity = 4'096;
    auto const insufficient{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, overflow_capacity)};
    EXPECT_EQ(insufficient.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 4'097, .two_to_64 = false}));
    EXPECT_EQ(insufficient.fields.front().relationship_minimum_required_bits, 13);
    EXPECT_EQ(insufficient.fields.front().relationship_width_sufficient, false);
    EXPECT_EQ(insufficient.fields.front().relationship_code_space_capacity_limit, 4'095);
    EXPECT_FALSE(insufficient.fields.front().relationship_capacity_headroom.has_value());
    EXPECT_FALSE(insufficient.diagnostics.empty());
}

TEST(PackedAnalyzer, DistinguishesCountCapacityAndExactTwoTo64) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::count_of, 12, 4'095)};
    std::array capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4'094, .byte_extent = std::nullopt}};

    auto const fits{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};
    EXPECT_EQ(fits.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 4'095, .two_to_64 = false}));
    EXPECT_EQ(fits.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 4'096, .two_to_64 = false}));
    EXPECT_EQ(fits.fields.front().relationship_minimum_required_bits, 12);
    EXPECT_EQ(fits.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(fits.fields.front().relationship_code_space_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_capacity_headroom, 0);
    EXPECT_EQ(fits.fields.front().relationship_semantic_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_sentinel_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_effective_capacity_limit, 4'094);
    EXPECT_EQ(fits.fields.front().relationship_effective_capacity_headroom, 0);
    EXPECT_TRUE(fits.diagnostics.empty());

    auto const full_fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::count_of, 64)};
    std::array const full_capacity{
        RelationshipTargetFacts{.target = full_fixture.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};
    auto const full{Analyzer::analyze_packed(full_fixture.types,
                                             full_fixture.packed,
                                             Variant{},
                                             AbiProfile::host_common(),
                                             1,
                                             full_capacity)};
    EXPECT_EQ(full.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_EQ(full.fields.front().relationship_minimum_required_bits, 64);
    EXPECT_EQ(full.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(full.fields.front().relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_EQ(full.fields.front().relationship_capacity_headroom, 0);
    EXPECT_FALSE(full.fields.front().relationship_semantic_capacity_limit.has_value());
    EXPECT_EQ(full.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(full.fields.front().relationship_effective_capacity_limit.has_value());
    EXPECT_FALSE(full.fields.front().relationship_effective_capacity_headroom.has_value());
    EXPECT_TRUE(full.diagnostics.empty());
}

TEST(PackedAnalyzer, KeepsMissingRelationshipTargetFactsUnknown) {
    auto const index{relationship_capacity_type(codegen::SemanticRelationKind::index_into, 1)};
    auto const missing{
        Analyzer::analyze_packed(index.types, index.packed, Variant{}, AbiProfile::host_common())};
    EXPECT_FALSE(missing.fields.front().relationship_target_extent.has_value());
    EXPECT_FALSE(missing.fields.front().relationship_minimum_required_bits.has_value());

    std::array const zero_capacity{RelationshipTargetFacts{
        .target = index.target, .element_capacity = 0, .byte_extent = std::nullopt}};
    auto const zero{Analyzer::analyze_packed(
        index.types, index.packed, Variant{}, AbiProfile::host_common(), 1, zero_capacity)};
    EXPECT_EQ(zero.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = false}));
    EXPECT_EQ(zero.fields.front().relationship_required_code_count,
              (ExactCodeCount{.value = 0, .two_to_64 = false}));
    EXPECT_EQ(zero.fields.front().relationship_minimum_required_bits, 1);
    EXPECT_EQ(zero.fields.front().relationship_width_sufficient, true);
    EXPECT_EQ(zero.fields.front().relationship_code_space_capacity_limit, 2);
    EXPECT_EQ(zero.fields.front().relationship_capacity_headroom, 2);
    EXPECT_FALSE(zero.fields.front().relationship_semantic_capacity_limit.has_value());
    EXPECT_EQ(zero.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)());
    EXPECT_FALSE(zero.fields.front().relationship_effective_capacity_limit.has_value());

    auto const offset{relationship_capacity_type(codegen::SemanticRelationKind::offset_into, 12)};
    std::array const offset_facts{RelationshipTargetFacts{
        .target = offset.target, .element_capacity = std::nullopt, .byte_extent = 1'000}};
    auto const missing_element_extent{Analyzer::analyze_packed(
        offset.types, offset.packed, Variant{}, AbiProfile::host_common(), 1, offset_facts)};
    EXPECT_FALSE(missing_element_extent.fields.front().relationship_target_extent.has_value());
    EXPECT_FALSE(
        missing_element_extent.fields.front().relationship_minimum_required_bits.has_value());
}

TEST(PackedAnalyzer, DerivesByteOffsetExtentAndWidth) {
    auto const fixture{relationship_capacity_type(codegen::SemanticRelationKind::offset_into,
                                                  13,
                                                  std::nullopt,
                                                  std::nullopt,
                                                  codegen::SemanticRelationUnit::bytes)};
    auto const target{Analyzer::analyze_soa(fixture.types,
                                            fixture.target,
                                            Variant{},
                                            AbiProfile::host_common(),
                                            8'192,
                                            SoaAllocationStrategy::separate_columns)};
    ASSERT_EQ(target.total_allocation_bytes, 8'192);
    std::array const facts{RelationshipTargetFacts{.target = fixture.target,
                                                   .element_capacity = target.capacity,
                                                   .byte_extent = target.total_allocation_bytes}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, facts)};

    ASSERT_EQ(analysis.fields.size(), 1);
    auto const& field{analysis.fields.front()};
    EXPECT_EQ(field.relationship_unit, codegen::SemanticRelationUnit::bytes);
    EXPECT_EQ(field.relationship_target_extent, 8'192);
    EXPECT_EQ(field.relationship_live_value_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_required_code_count,
              (ExactCodeCount{.value = 8'192, .two_to_64 = false}));
    EXPECT_EQ(field.relationship_minimum_required_bits, 13);
    EXPECT_EQ(field.relationship_width_sufficient, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AccountsForMultipleSentinelsInCapacityHeadroom) {
    auto const fixture{
        relationship_capacity_type(codegen::SemanticRelationKind::index_into, 3, 6, 7)};
    std::array const capacity{RelationshipTargetFacts{
        .target = fixture.target, .element_capacity = 4, .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    ASSERT_EQ(analysis.fields.size(), 1);
    EXPECT_EQ(analysis.fields.front().relationship_code_space_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_capacity_headroom, 2);
    EXPECT_EQ(analysis.fields.front().relationship_semantic_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_sentinel_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_limit, 6);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_headroom, 2);
    EXPECT_EQ(analysis.fields.front().relationship_width_sufficient, true);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, DiagnosesRelationshipRequirementBeyond64Bits) {
    auto const fixture{relationship_capacity_type(
        codegen::SemanticRelationKind::count_of, 64, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const capacity{
        RelationshipTargetFacts{.target = fixture.target,
                                .element_capacity = (std::numeric_limits<std::uint64_t>::max)(),
                                .byte_extent = std::nullopt}};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.packed, Variant{}, AbiProfile::host_common(), 1, capacity)};

    EXPECT_EQ(analysis.fields.front().relationship_live_value_count,
              (ExactCodeCount{.value = 0, .two_to_64 = true}));
    EXPECT_FALSE(analysis.fields.front().relationship_required_code_count.has_value());
    EXPECT_EQ(analysis.fields.front().relationship_minimum_required_bits, 65);
    EXPECT_EQ(analysis.fields.front().relationship_width_sufficient, false);
    EXPECT_EQ(analysis.fields.front().relationship_code_space_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(analysis.fields.front().relationship_capacity_headroom.has_value());
    EXPECT_EQ(analysis.fields.front().relationship_semantic_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(analysis.fields.front().relationship_sentinel_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_EQ(analysis.fields.front().relationship_effective_capacity_limit,
              (std::numeric_limits<std::uint64_t>::max)() - 1);
    EXPECT_FALSE(analysis.fields.front().relationship_effective_capacity_headroom.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsUnusedAndExcessBits) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;
    auto analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, variant, AbiProfile::host_common(), 100)};
    EXPECT_EQ(analysis.bits_used, 28);
    EXPECT_EQ(analysis.unused_bits, 4);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 400);

    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 25;
    analysis =
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common());
    EXPECT_FALSE(analysis.unused_bits.has_value());
    EXPECT_EQ(analysis.overflow_bits, 1);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSchemaAndOverrideProvenance) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.schema_storage_type, "std::uint32_t");
    EXPECT_TRUE(analysis.storage_overridden);
    EXPECT_EQ(analysis.fields[0].logical_type, "std::uint32_t");
    EXPECT_EQ(analysis.fields[0].schema_bit_width, 24);
    EXPECT_EQ(analysis.fields[0].bit_width, 20);
    EXPECT_TRUE(analysis.fields[0].overridden);
    EXPECT_FALSE(analysis.fields[1].overridden);
}

TEST(PackedAnalyzer, DiagnosesZeroWidthFields) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 0;
    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_FALSE(analysis.fields[0].most_significant_bit.has_value());
    EXPECT_EQ(analysis.bits_used, 8);
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, AppliesExplicitVariantOverrides) {
    auto const fixture{entity_id_type()};
    Variant variant;
    variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 40;

    auto const analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, variant, AbiProfile::host_common())};

    EXPECT_EQ(analysis.storage_type, "std::uint64_t");
    EXPECT_EQ(analysis.storage_bits, 64);
    EXPECT_EQ(analysis.bits_used, 48);
    EXPECT_EQ(analysis.unused_bits, 16);
}

TEST(PackedAnalyzer, ReportsOverflowSafeAggregateMemoryAtSelectedScale) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common(), 1'000)};

    EXPECT_EQ(analysis.aggregate.element_count, 1'000);
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 4'000);
    EXPECT_EQ(analysis.aggregate.total_payload_bits, 32'000);
    EXPECT_EQ(analysis.aggregate.total_unused_bits, 0);
    EXPECT_EQ(analysis.aggregate.cache_line_bytes, 64);
    EXPECT_EQ(analysis.aggregate.minimum_cache_lines, 63);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 16);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.page_bytes, 4'096);
    EXPECT_EQ(analysis.aggregate.minimum_pages, 1);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 1'024);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);
}

TEST(PackedAnalyzer, ReportsAlignedContiguousBoundaryStraddling) {
    auto const fixture{entity_id_type()};
    auto abi{AbiProfile::host_common()};
    abi.set("uint24",
            {.size_bytes = 3,
             .alignment_bytes = 1,
             .integer_signed = false,
             .unsigned_value_bits = 24,
             .provenance = "test"});
    abi.set("wide",
            {.size_bytes = 16,
             .alignment_bytes = 1,
             .integer_signed = false,
             .unsigned_value_bits = 128,
             .provenance = "test"});
    abi.set_memory_facts({.cache_line_bytes = 8,
                          .page_bytes = 10,
                          .l1_data_cache_bytes = std::nullopt,
                          .l2_cache_bytes = std::nullopt,
                          .l3_cache_bytes = std::nullopt,
                          .provenance = "test"});

    Variant three_byte_variant;
    three_byte_variant.overrides.packed_storage_types[fixture.type] = "uint24";
    three_byte_variant.overrides
        .packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 16;
    auto analysis{
        Analyzer::analyze_packed(fixture.types, fixture.type, three_byte_variant, abi, 10)};
    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 30);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 2);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 2);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 3);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 2);

    analysis = Analyzer::analyze_packed(fixture.types, fixture.type, three_byte_variant, abi, 1);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 0);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 0);

    Variant wide_variant;
    wide_variant.overrides.packed_storage_types[fixture.type] = "wide";
    analysis = Analyzer::analyze_packed(fixture.types, fixture.type, wide_variant, abi, 3);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_cache_line, 0);
    EXPECT_EQ(analysis.aggregate.cache_line_straddling_elements, 3);
    EXPECT_EQ(analysis.aggregate.complete_elements_per_page, 0);
    EXPECT_EQ(analysis.aggregate.page_straddling_elements, 3);
}

TEST(PackedAnalyzer, KeepsUnknownTargetMemoryFactsUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = {}});

    auto const analysis{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};

    EXPECT_EQ(analysis.aggregate.total_storage_bytes, 40);
    EXPECT_FALSE(analysis.aggregate.cache_line_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_line_straddling_elements.has_value());
    EXPECT_FALSE(analysis.aggregate.page_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.page_straddling_elements.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSignedArbitraryWidthRange) {
    auto const fixture{signed_delta_type()};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    EXPECT_EQ(analysis.fields[0].kind, codegen::PackedFieldKind::signed_integer);
    EXPECT_EQ(analysis.fields[0].bit_width, 17U);
    EXPECT_EQ(analysis.fields[0].minimum_signed_value, -65'536);
    EXPECT_EQ(analysis.fields[0].maximum_signed_value, 65'535);
    EXPECT_FALSE(analysis.fields[1].minimum_signed_value.has_value());
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsSignedSemanticRangeSentinelAndDerivedWidth) {
    auto const fixture{signed_delta_type(true)};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 2U);
    auto const& delta{analysis.fields[0]};
    EXPECT_TRUE(delta.schema_bit_width_auto);
    EXPECT_EQ(delta.bit_width, 8U);
    EXPECT_EQ(delta.minimum_semantic_value, codegen::PackedIntegerValue{-100});
    EXPECT_EQ(delta.maximum_semantic_value, codegen::PackedIntegerValue{100});
    EXPECT_EQ(delta.semantic_value_count, 201U);
    EXPECT_EQ(delta.sentinel_code_count, 1U);
    EXPECT_EQ(delta.required_code_count, 202U);
    EXPECT_EQ(delta.minimum_required_bits, 8U);
    EXPECT_EQ(delta.unused_codes, 54U);
    ASSERT_EQ(delta.named_codes.size(), 1U);
    EXPECT_EQ(delta.named_codes[0].value, codegen::PackedIntegerValue{-128});
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, ReportsKnownZeroWasteForFullSigned64BitDomain) {
    auto const fixture{signed_delta_type(false, true)};

    auto const analysis{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, AbiProfile::host_common())};

    ASSERT_EQ(analysis.fields.size(), 1U);
    auto const& delta{analysis.fields[0]};
    EXPECT_TRUE(delta.schema_bit_width_auto);
    EXPECT_EQ(delta.bit_width, 64U);
    EXPECT_FALSE(delta.semantic_value_count.has_value());
    EXPECT_FALSE(delta.required_code_count.has_value());
    EXPECT_EQ(delta.minimum_required_bits, 64U);
    EXPECT_EQ(delta.unused_codes, 0U);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAnalyzer, DiagnosesAggregateOverflow) {
    auto const fixture{entity_id_type()};
    auto const analysis{Analyzer::analyze_packed(fixture.types,
                                                 fixture.type,
                                                 Variant{},
                                                 AbiProfile::host_common(),
                                                 std::numeric_limits<std::uint64_t>::max())};

    EXPECT_FALSE(analysis.aggregate.total_storage_bytes.has_value());
    EXPECT_FALSE(analysis.aggregate.total_payload_bits.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_cache_lines.has_value());
    EXPECT_FALSE(analysis.aggregate.minimum_pages.has_value());
    EXPECT_FALSE(analysis.aggregate.cache_line_straddling_elements.has_value());
    EXPECT_FALSE(analysis.aggregate.page_straddling_elements.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, SeparatesSelectedBitsFromWholeStorageAndRegionFootprints) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 100)};
    std::array const accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read},
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi, 3)};

    EXPECT_EQ(analysis.field_names, (std::vector<std::string>{"index", "entity_type"}));
    EXPECT_EQ(analysis.element_count, 100);
    EXPECT_EQ(analysis.multiplicity, 3);
    EXPECT_EQ(analysis.useful_bits, 3'200);
    EXPECT_EQ(analysis.read_useful_bits, 2'400);
    EXPECT_EQ(analysis.write_useful_bits, 800);
    EXPECT_EQ(analysis.logical_read_useful_bits, 7'200);
    EXPECT_EQ(analysis.logical_write_useful_bits, 2'400);
    ASSERT_EQ(analysis.fields.size(), 2);
    EXPECT_EQ(analysis.fields[0].name, "index");
    EXPECT_EQ(analysis.fields[0].operation, AccessOperation::read);
    EXPECT_EQ(analysis.fields[0].bit_width, 24);
    EXPECT_EQ(analysis.fields[0].useful_bits, 2'400);
    EXPECT_EQ(analysis.fields[0].read_useful_bits, 2'400);
    EXPECT_EQ(analysis.fields[0].write_useful_bits, 0);
    EXPECT_EQ(analysis.fields[0].logical_read_useful_bits, 7'200);
    EXPECT_EQ(analysis.fields[0].logical_write_useful_bits, 0);
    EXPECT_EQ(analysis.fields[1].name, "entity_type");
    EXPECT_EQ(analysis.fields[1].operation, AccessOperation::write);
    EXPECT_EQ(analysis.fields[1].bit_width, 8);
    EXPECT_EQ(analysis.fields[1].read_useful_bits, 0);
    EXPECT_EQ(analysis.fields[1].write_useful_bits, 800);
    EXPECT_EQ(analysis.fields[1].logical_write_useful_bits, 2'400);
    EXPECT_EQ(analysis.storage_footprint_bytes, 400);
    EXPECT_EQ(analysis.storage_footprint_bits, 3'200);
    EXPECT_EQ(analysis.non_useful_storage_bits, 0);
    EXPECT_EQ(analysis.unselected_field_bits, 0);
    EXPECT_EQ(analysis.reserved_region_bits, 0);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 0);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 7);
    EXPECT_EQ(analysis.minimum_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.read_cache_lines_touched, 7);
    EXPECT_EQ(analysis.read_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.write_cache_lines_touched, 7);
    EXPECT_EQ(analysis.write_cache_bytes_touched, 448);
    EXPECT_EQ(analysis.minimum_pages_touched, 1);
    EXPECT_EQ(analysis.minimum_page_bytes_touched, 4'096);
    EXPECT_EQ(analysis.read_pages_touched, 1);
    EXPECT_EQ(analysis.write_pages_touched, 1);
    EXPECT_EQ(analysis.cache_footprint_capacity.working_set_bytes, 448);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, ReportsUnselectedAndUnusedStorageBitsAsNonUseful) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    Variant variant;
    variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] = 20;
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, variant, abi, 10)};
    std::array const accesses{
        AccessIntent{.name = "entity_type", .operation = AccessOperation::read_write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.useful_bits, 80);
    EXPECT_EQ(analysis.read_useful_bits, 80);
    EXPECT_EQ(analysis.write_useful_bits, 80);
    EXPECT_EQ(analysis.storage_footprint_bits, 320);
    EXPECT_EQ(analysis.non_useful_storage_bits, 240);
    EXPECT_EQ(analysis.unselected_field_bits, 200);
    EXPECT_EQ(analysis.reserved_region_bits, 0);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 40);
    EXPECT_EQ(analysis.minimum_cache_lines_touched, 1);
    EXPECT_EQ(analysis.read_cache_lines_touched, 1);
    EXPECT_EQ(analysis.write_cache_lines_touched, 1);
    EXPECT_TRUE(analysis.diagnostics.empty());
}

TEST(PackedAccessAnalyzer, RejectsReservedMissingAndConflictingSelections) {
    auto const fixture{entity_id_type(true)};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 2)};
    std::array const accesses{AccessIntent{.name = "future", .operation = AccessOperation::read},
                              AccessIntent{.name = "missing", .operation = AccessOperation::read},
                              AccessIntent{.name = "index", .operation = AccessOperation::read},
                              AccessIntent{.name = "index", .operation = AccessOperation::write}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.field_names, (std::vector<std::string>{"index"}));
    EXPECT_EQ(analysis.useful_bits, 40);
    EXPECT_EQ(analysis.non_useful_storage_bits, 24);
    EXPECT_EQ(analysis.unselected_field_bits, 16);
    EXPECT_EQ(analysis.reserved_region_bits, 8);
    EXPECT_EQ(analysis.physically_unused_storage_bits, 0);
    ASSERT_EQ(analysis.diagnostics.size(), 3);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::error);
    EXPECT_EQ(analysis.diagnostics[2].severity, DiagnosticSeverity::error);
}

TEST(PackedAccessAnalyzer, KeepsUnknownMemoryFactsUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi)};

    EXPECT_EQ(analysis.useful_bits, 240);
    EXPECT_EQ(analysis.storage_footprint_bytes, 40);
    EXPECT_EQ(analysis.non_useful_storage_bits, 80);
    EXPECT_FALSE(analysis.cache_line_bytes.has_value());
    EXPECT_FALSE(analysis.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(analysis.minimum_cache_bytes_touched.has_value());
    EXPECT_FALSE(analysis.page_bytes.has_value());
    EXPECT_FALSE(analysis.minimum_pages_touched.has_value());
    ASSERT_EQ(analysis.diagnostics.size(), 2);
    EXPECT_EQ(analysis.diagnostics[0].severity, DiagnosticSeverity::warning);
    EXPECT_EQ(analysis.diagnostics[1].severity, DiagnosticSeverity::warning);
}

TEST(PackedAccessAnalyzer, DiagnosesZeroMultiplicityAndCheckedOverflow) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, abi, (std::numeric_limits<std::uint64_t>::max)())};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const analysis{Analyzer::analyze_packed_access(packed, accesses, abi, 0)};

    EXPECT_FALSE(analysis.useful_bits.has_value());
    EXPECT_FALSE(analysis.storage_footprint_bytes.has_value());
    EXPECT_FALSE(analysis.logical_read_useful_bits.has_value());
    EXPECT_FALSE(analysis.minimum_cache_lines_touched.has_value());
    EXPECT_FALSE(analysis.minimum_pages_touched.has_value());
    ASSERT_EQ(analysis.fields.size(), 1);
    EXPECT_FALSE(analysis.fields[0].useful_bits.has_value());
    EXPECT_FALSE(analysis.fields[0].logical_read_useful_bits.has_value());
    EXPECT_FALSE(analysis.unselected_field_bits.has_value());
    EXPECT_FALSE(analysis.reserved_region_bits.has_value());
    EXPECT_FALSE(analysis.physically_unused_storage_bits.has_value());
    EXPECT_FALSE(analysis.diagnostics.empty());
}

TEST(PackedAccessComparison, ReportsWidthAndStorageConsequencesForOneWorkload) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};

    auto const baseline_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 100)};
    Variant narrow_variant;
    narrow_variant.overrides.packed_field_widths[{.type = fixture.type, .field_name = "index"}] =
        20;
    auto const narrow_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, narrow_variant, abi, 100)};
    auto const baseline_access{Analyzer::analyze_packed_access(baseline_packed, accesses, abi, 2)};
    auto const narrow_access{Analyzer::analyze_packed_access(narrow_packed, accesses, abi, 2)};

    auto const width_comparison{Analyzer::compare_packed_access(baseline_access, narrow_access)};

    EXPECT_TRUE(width_comparison.diagnostics.empty());
    EXPECT_EQ(width_comparison.field_names, (std::vector<std::string>{"index"}));
    EXPECT_EQ(width_comparison.element_count, 100);
    EXPECT_EQ(width_comparison.multiplicity, 2);
    ASSERT_TRUE(width_comparison.useful_bit_delta.has_value());
    EXPECT_EQ(width_comparison.useful_bit_delta->direction, NumericDeltaDirection::decreased);
    EXPECT_EQ(width_comparison.useful_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.logical_read_useful_bit_delta->magnitude, 800);
    ASSERT_EQ(width_comparison.fields.size(), 1);
    EXPECT_EQ(width_comparison.fields[0].name, "index");
    EXPECT_EQ(width_comparison.fields[0].first.bit_width, 24);
    EXPECT_EQ(width_comparison.fields[0].second.bit_width, 20);
    EXPECT_EQ(width_comparison.fields[0].bit_width_delta->direction,
              NumericDeltaDirection::decreased);
    EXPECT_EQ(width_comparison.fields[0].bit_width_delta->magnitude, 4);
    EXPECT_EQ(width_comparison.fields[0].useful_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.fields[0].logical_read_useful_bit_delta->magnitude, 800);
    EXPECT_EQ(width_comparison.storage_footprint_byte_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.non_useful_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(width_comparison.non_useful_storage_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.unselected_field_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.reserved_region_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(width_comparison.physically_unused_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(width_comparison.physically_unused_storage_bit_delta->magnitude, 400);
    EXPECT_EQ(width_comparison.cache_line_delta->direction, NumericDeltaDirection::unchanged);

    Variant wide_storage_variant;
    wide_storage_variant.overrides.packed_storage_types[fixture.type] = "std::uint64_t";
    auto const wide_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, wide_storage_variant, abi, 100)};
    auto const wide_access{Analyzer::analyze_packed_access(wide_packed, accesses, abi, 2)};
    auto const storage_comparison{Analyzer::compare_packed_access(baseline_access, wide_access)};

    EXPECT_EQ(storage_comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(storage_comparison.storage_footprint_byte_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.storage_footprint_byte_delta->magnitude, 400);
    EXPECT_EQ(storage_comparison.unselected_field_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    EXPECT_EQ(storage_comparison.physically_unused_storage_bit_delta->direction,
              NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.physically_unused_storage_bit_delta->magnitude, 3'200);
    EXPECT_EQ(storage_comparison.cache_line_delta->direction, NumericDeltaDirection::increased);
    EXPECT_EQ(storage_comparison.cache_line_delta->magnitude, 6);
    EXPECT_EQ(storage_comparison.page_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, ReportsTargetStorageAndRegionConsequencesForOneWorkload) {
    auto const fixture{entity_id_type()};
    auto first_target{AbiProfile::host_common()};
    first_target.set_memory_facts({.cache_line_bytes = 64,
                                   .page_bytes = 4'096,
                                   .l1_data_cache_bytes = 500,
                                   .l2_cache_bytes = std::nullopt,
                                   .l3_cache_bytes = std::nullopt,
                                   .provenance = "Test first memory"});
    auto second_target{AbiProfile::host_common()};
    second_target.set("std::uint32_t",
                      {.size_bytes = 8,
                       .alignment_bytes = 8,
                       .integer_signed = false,
                       .unsigned_value_bits = 32,
                       .provenance = "Test wide target"});
    second_target.set_memory_facts({.cache_line_bytes = 128,
                                    .page_bytes = 8'192,
                                    .l1_data_cache_bytes = 500,
                                    .l2_cache_bytes = std::nullopt,
                                    .l3_cache_bytes = std::nullopt,
                                    .provenance = "Test second memory"});
    std::array const accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read_write}};
    auto const first_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, first_target, 100)};
    auto const second_packed{
        Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, second_target, 100)};
    auto const first{Analyzer::analyze_packed_access(first_packed, accesses, first_target, 2)};
    auto const second{Analyzer::analyze_packed_access(second_packed, accesses, second_target, 2)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};
    auto const identical{Analyzer::compare_packed_access(first, first)};

    EXPECT_EQ(comparison.first.type, fixture.type);
    EXPECT_EQ(comparison.first.useful_bits, 2'400);
    EXPECT_EQ(comparison.second.useful_bits, 2'400);
    ASSERT_TRUE(comparison.useful_bit_delta.has_value());
    EXPECT_EQ(comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.logical_read_useful_bit_delta.has_value());
    EXPECT_EQ(comparison.logical_read_useful_bit_delta->direction,
              NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.storage_footprint_byte_delta.has_value());
    EXPECT_EQ(comparison.storage_footprint_byte_delta->magnitude, 400);
    ASSERT_TRUE(comparison.non_useful_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.non_useful_storage_bit_delta->magnitude, 3'200);
    ASSERT_TRUE(comparison.unselected_field_bit_delta.has_value());
    EXPECT_EQ(comparison.unselected_field_bit_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(comparison.physically_unused_storage_bit_delta.has_value());
    EXPECT_EQ(comparison.physically_unused_storage_bit_delta->magnitude, 3'200);
    ASSERT_TRUE(comparison.cache_line_size_delta.has_value());
    EXPECT_EQ(comparison.cache_line_size_delta->magnitude, 64);
    ASSERT_TRUE(comparison.cache_byte_delta.has_value());
    EXPECT_EQ(comparison.cache_byte_delta->magnitude, 448);
    ASSERT_TRUE(comparison.page_size_delta.has_value());
    EXPECT_EQ(comparison.page_size_delta->magnitude, 4'096);
    ASSERT_TRUE(comparison.page_byte_delta.has_value());
    EXPECT_EQ(comparison.page_byte_delta->magnitude, 4'096);
    EXPECT_EQ(comparison.first.cache_footprint_capacity.fits_l1_data, true);
    EXPECT_EQ(comparison.second.cache_footprint_capacity.fits_l1_data, false);
    EXPECT_TRUE(comparison.diagnostics.empty());

    ASSERT_TRUE(identical.storage_footprint_byte_delta.has_value());
    EXPECT_EQ(identical.storage_footprint_byte_delta->direction, NumericDeltaDirection::unchanged);
    ASSERT_TRUE(identical.cache_byte_delta.has_value());
    EXPECT_EQ(identical.cache_byte_delta->direction, NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, MatchesFieldDetailsIndependentOfIntentOrder) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const first_accesses{
        AccessIntent{.name = "index", .operation = AccessOperation::read},
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write}};
    std::array const second_accesses{
        AccessIntent{.name = "entity_type", .operation = AccessOperation::write},
        AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const first{Analyzer::analyze_packed_access(packed, first_accesses, abi, 3)};
    auto const second{Analyzer::analyze_packed_access(packed, second_accesses, abi, 3)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_TRUE(comparison.diagnostics.empty());
    ASSERT_EQ(comparison.fields.size(), 2);
    EXPECT_EQ(comparison.fields[0].name, "index");
    EXPECT_EQ(comparison.fields[1].name, "entity_type");
    EXPECT_EQ(comparison.fields[0].useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.fields[1].logical_write_useful_bit_delta->direction,
              NumericDeltaDirection::unchanged);
}

TEST(PackedAccessComparison, DiagnosesMissingFieldDetails) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto second{first};
    second.fields.clear();

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_TRUE(comparison.fields.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
}

TEST(PackedAccessComparison, RejectsMismatchedWorkloads) {
    auto first{PackedAccessAnalysis{}};
    first.field_names = {"index"};
    first.accesses = {{.name = "index", .operation = AccessOperation::read}};
    first.element_count = 10;
    first.multiplicity = 1;
    first.useful_bits = 100;
    auto second{first};
    second.accesses.front().operation = AccessOperation::write;

    auto comparison{Analyzer::compare_packed_access(first, second)};
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());

    second.accesses = first.accesses;
    second.element_count = 11;
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.storage_footprint_byte_delta.has_value());

    second.element_count = first.element_count;
    second.multiplicity = 2;
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.logical_read_useful_bit_delta.has_value());

    second = first;
    second.type = lispb::schema::TypeId{first.type.value + 1};
    comparison = Analyzer::compare_packed_access(first, second);
    EXPECT_FALSE(comparison.diagnostics.empty());
    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
    EXPECT_NE(comparison.diagnostics.back().message.find("different packed values"),
              std::string::npos);
}

TEST(PackedAccessComparison, KeepsUnknownPhysicalDeltasUnknown) {
    auto const fixture{entity_id_type()};
    AbiProfile abi{"unknown memory"};
    abi.set("std::uint32_t",
            {.size_bytes = 4,
             .alignment_bytes = 4,
             .integer_signed = false,
             .unsigned_value_bits = 32,
             .provenance = "test"});
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const packed{Analyzer::analyze_packed(fixture.types, fixture.type, Variant{}, abi, 10)};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto const second{Analyzer::analyze_packed_access(packed, accesses, abi)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_EQ(comparison.useful_bit_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_EQ(comparison.storage_footprint_byte_delta->direction, NumericDeltaDirection::unchanged);
    EXPECT_FALSE(comparison.cache_line_delta.has_value());
    EXPECT_FALSE(comparison.cache_byte_delta.has_value());
    EXPECT_FALSE(comparison.page_delta.has_value());
    EXPECT_FALSE(comparison.page_byte_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

TEST(PackedAccessComparison, KeepsOverflowedDeltasUnknown) {
    auto const fixture{entity_id_type()};
    auto const abi{AbiProfile::host_common()};
    std::array const accesses{AccessIntent{.name = "index", .operation = AccessOperation::read}};
    auto const packed{Analyzer::analyze_packed(
        fixture.types, fixture.type, Variant{}, abi, (std::numeric_limits<std::uint64_t>::max)())};
    auto const first{Analyzer::analyze_packed_access(packed, accesses, abi)};
    auto const second{Analyzer::analyze_packed_access(packed, accesses, abi)};

    auto const comparison{Analyzer::compare_packed_access(first, second)};

    EXPECT_FALSE(comparison.useful_bit_delta.has_value());
    EXPECT_FALSE(comparison.storage_footprint_byte_delta.has_value());
    EXPECT_FALSE(comparison.cache_line_delta.has_value());
    EXPECT_FALSE(comparison.page_delta.has_value());
    EXPECT_FALSE(comparison.diagnostics.empty());
}

} // namespace
} // namespace ioj::layout
