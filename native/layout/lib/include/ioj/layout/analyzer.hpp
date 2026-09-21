#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/schema/enum_domain.h>
#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout {

enum class NumericDeltaDirection { unchanged, increased, decreased };

struct NumericDelta {
    NumericDeltaDirection direction{NumericDeltaDirection::unchanged};
    std::uint64_t magnitude{};
    std::optional<double> percentage;
};

struct CacheCapacityAnalysis {
    std::optional<std::uint64_t> working_set_bytes;
    std::optional<std::uint64_t> l1_data_capacity_bytes;
    std::optional<std::uint64_t> l2_capacity_bytes;
    std::optional<std::uint64_t> l3_capacity_bytes;
    std::optional<bool> fits_l1_data;
    std::optional<bool> fits_l2;
    std::optional<bool> fits_l3;
};

auto numeric_delta(std::optional<std::uint64_t> baseline, std::optional<std::uint64_t> variant)
    -> std::optional<NumericDelta>;

using EnumCodeValue = lispb::schema::EnumCode;

struct EnumeratorAnalysis {
    std::string name;
    bool sentinel{};
    bool count_sentinel{};
    std::optional<EnumCodeValue> code;
};

struct EnumBackingAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> cache_line_straddling_elements;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<std::uint64_t> page_straddling_elements;
    CacheCapacityAnalysis cache_capacity;
};

struct EnumDomainAnalysis {
    lispb::schema::TypeId type;
    std::string backing_type;
    std::optional<TypeFacts> backing_facts;
    std::optional<std::uint64_t> backing_bits;
    std::uint64_t live_value_count{};
    std::uint64_t reserved_value_count{};
    std::optional<EnumCodeValue> minimum_value;
    std::optional<EnumCodeValue> maximum_value;
    std::optional<bool> signed_domain;
    std::optional<bool> declared_signedness;
    std::optional<std::uint32_t> minimum_required_bits;
    std::optional<std::uint32_t> declared_bit_width;
    std::optional<std::uint32_t> effective_bit_width;
    std::optional<bool> semantic_width_can_represent_domain;
    std::optional<std::uint64_t> unused_semantic_codes;
    std::optional<bool> backing_can_represent_domain;
    std::optional<std::uint64_t> unused_backing_codes;
    std::vector<EnumeratorAnalysis> enumerators;
    std::vector<Diagnostic> diagnostics;
    EnumBackingAggregateAnalysis aggregate;
};

struct EnumTargetComparison {
    EnumDomainAnalysis first;
    EnumDomainAnalysis second;
    std::optional<NumericDelta> backing_size_delta;
    std::optional<NumericDelta> backing_alignment_delta;
    std::optional<NumericDelta> backing_value_bit_delta;
    std::optional<NumericDelta> backing_bit_delta;
    std::optional<NumericDelta> unused_backing_code_delta;
    std::optional<bool> backing_fit_changed;
    std::optional<NumericDelta> total_storage_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> complete_elements_per_cache_line_delta;
    std::optional<NumericDelta> cache_line_straddling_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
    std::optional<NumericDelta> page_straddling_delta;
    std::vector<Diagnostic> diagnostics;
};

struct PackedNamedCodeAnalysis {
    std::string name;
    codegen::PackedIntegerValue value;
    bool sentinel{};
};

struct ExactCodeCount {
    std::uint64_t value{};
    bool two_to_64{};

    auto operator==(ExactCodeCount const&) const -> bool = default;
};

struct IntegerScalarAnalysis {
    lispb::schema::TypeId type;
    bool signedness{};
    codegen::PackedIntegerValue minimum_value;
    codegen::PackedIntegerValue maximum_value;
    std::optional<std::uint64_t> live_value_count;
    std::uint64_t sentinel_code_count{};
    std::optional<std::uint64_t> required_code_count;
    std::uint32_t minimum_required_bits{};
    std::optional<std::uint32_t> declared_bit_width;
    std::uint32_t effective_bit_width{};
    std::optional<std::uint64_t> unused_codes;
    std::vector<PackedNamedCodeAnalysis> named_codes;
    std::optional<codegen::SemanticRelationKind> relationship_kind;
    std::optional<codegen::SemanticRelationUnit> relationship_unit;
    std::optional<std::string> relationship_target;
    std::optional<std::uint64_t> relationship_target_extent;
    std::optional<ExactCodeCount> relationship_live_value_count;
    std::optional<ExactCodeCount> relationship_required_code_count;
    std::optional<std::uint32_t> relationship_minimum_required_bits;
    std::optional<bool> relationship_width_sufficient;
    std::optional<std::uint64_t> relationship_code_space_capacity_limit;
    std::optional<std::uint64_t> relationship_capacity_headroom;
    std::optional<std::uint64_t> relationship_semantic_capacity_limit;
    std::optional<std::uint64_t> relationship_sentinel_capacity_limit;
    std::optional<std::uint64_t> relationship_effective_capacity_limit;
    std::optional<std::uint64_t> relationship_effective_capacity_headroom;
    std::vector<Diagnostic> diagnostics;
};

struct IntegerScalarCapacityComparison {
    IntegerScalarAnalysis first;
    IntegerScalarAnalysis second;
    std::optional<NumericDelta> relationship_target_extent_delta;
    std::optional<NumericDelta> relationship_minimum_required_bit_delta;
    std::optional<NumericDelta> relationship_code_space_capacity_limit_delta;
    std::optional<NumericDelta> relationship_capacity_headroom_delta;
    std::optional<NumericDelta> relationship_semantic_capacity_limit_delta;
    std::optional<NumericDelta> relationship_sentinel_capacity_limit_delta;
    std::optional<NumericDelta> relationship_effective_capacity_limit_delta;
    std::optional<NumericDelta> relationship_effective_capacity_headroom_delta;
    std::optional<bool> relationship_width_fit_changed;
};

struct OptionalSentinelAnalysis {
    lispb::schema::TypeId type;
    lispb::schema::TypeId source_type;
    std::string sentinel_name;
    codegen::PackedIntegerValue sentinel_value;
    bool source_signedness{};
    codegen::PackedIntegerValue source_minimum;
    codegen::PackedIntegerValue source_maximum;
    std::optional<std::uint64_t> present_value_count;
    std::uint64_t absence_code_count{1};
    std::uint64_t other_sentinel_code_count{};
    std::optional<std::uint64_t> unused_code_count;
    std::uint32_t encoded_storage_bits{};
    ExactCodeCount total_code_count;
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits;
    std::vector<Diagnostic> diagnostics;
};

struct OptionalPresenceBitAnalysis {
    lispb::schema::TypeId type;
    lispb::schema::TypeId source_type;
    bool source_signedness{};
    codegen::PackedIntegerValue source_minimum;
    codegen::PackedIntegerValue source_maximum;
    std::optional<std::uint64_t> present_value_count;
    std::uint64_t canonical_absence_state_count{1};
    std::uint64_t source_sentinel_code_count{};
    std::optional<std::uint64_t> source_unused_payload_codes;
    std::uint32_t presence_bits{1};
    std::uint32_t payload_bits{};
    std::uint32_t encoded_storage_bits{};
    std::uint64_t noncanonical_absence_patterns{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits;
    std::vector<Diagnostic> diagnostics;
};

enum class OptionalEncodingKind { sentinel, presence_bit };

struct OptionalEncodingSummary {
    lispb::schema::TypeId type;
    lispb::schema::TypeId source_type;
    OptionalEncodingKind kind{OptionalEncodingKind::sentinel};
    std::optional<std::string> absence_sentinel_name;
    std::optional<std::uint64_t> present_value_count;
    std::uint64_t canonical_absence_state_count{1};
    std::uint64_t source_sentinel_code_count{};
    std::uint64_t source_sentinel_codes_used_for_absence{};
    std::uint64_t remaining_source_sentinel_code_count{};
    std::optional<std::uint64_t> source_unused_payload_codes;
    std::uint64_t noncanonical_absence_patterns{};
    std::uint32_t encoded_storage_bits{};
    std::optional<std::uint64_t> total_encoded_bits;
};

struct OptionalEncodingComparison {
    OptionalEncodingSummary first;
    OptionalEncodingSummary second;
    std::uint64_t element_count{1};
    bool supported_encodings{};
    bool compatible_source{};
    std::optional<NumericDelta> encoded_storage_bit_delta;
    std::optional<NumericDelta> total_encoded_bit_delta;
    std::vector<Diagnostic> diagnostics;
};

struct LinearQuantizedAnalysis {
    lispb::schema::TypeId type;
    lispb::schema::TypeId source_type;
    codegen::PackedIntegerValue source_minimum;
    codegen::PackedIntegerValue source_maximum;
    std::optional<std::uint64_t> source_span;
    std::uint32_t encoded_storage_bits{};
    ExactCodeCount total_code_count;
    std::uint64_t reserved_code_count{};
    ExactCodeCount usable_code_count;
    long double resolution{};
    long double maximum_rounding_error{};
    bool minimum_endpoint_exact{};
    bool maximum_endpoint_exact{};
    codegen::QuantizationClipping clipping{codegen::QuantizationClipping::reject};
};

struct LinearQuantizedComparison {
    LinearQuantizedAnalysis first;
    LinearQuantizedAnalysis second;
    std::uint64_t element_count{1};
    bool compatible_source{};
    std::optional<NumericDelta> encoded_storage_bit_delta;
    std::optional<NumericDelta> total_code_count_delta;
    std::optional<NumericDelta> usable_code_count_delta;
    std::optional<NumericDelta> reserved_code_count_delta;
    std::optional<bool> clipping_changed;
    std::optional<long double> resolution_delta;
    std::optional<long double> maximum_rounding_error_delta;
    std::optional<std::uint64_t> first_total_encoded_bits;
    std::optional<std::uint64_t> second_total_encoded_bits;
    std::optional<NumericDelta> total_encoded_bit_delta;
    std::vector<Diagnostic> diagnostics;
};

struct FixedPointAnalysis {
    lispb::schema::TypeId type;
    bool signedness{};
    std::uint32_t total_bits{};
    std::uint32_t fractional_bits{};
    std::uint32_t whole_bits{};
    codegen::PackedIntegerValue minimum_raw_value;
    codegen::PackedIntegerValue maximum_raw_value;
    long double scale{};
    long double resolution{};
    long double minimum_value{};
    long double maximum_value{};
    long double maximum_rounding_error{};
    codegen::FixedPointRounding rounding{codegen::FixedPointRounding::nearest_even};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits;
    std::vector<Diagnostic> diagnostics;
};

struct MiniFloatAnalysis {
    lispb::schema::TypeId type;
    std::uint32_t sign_bits{};
    std::uint32_t exponent_bits{};
    std::uint32_t significand_bits{};
    std::uint32_t total_bits{};
    std::int32_t exponent_bias{};
    std::uint64_t exponent_code_count{};
    std::uint64_t normal_exponent_code_count{};
    std::int32_t minimum_normal_exponent{};
    std::int32_t maximum_normal_exponent{};
    ExactCodeCount total_code_count;
    std::uint64_t zero_code_count{};
    std::uint64_t infinity_code_count{};
    std::uint64_t nan_code_count{};
    std::uint64_t nonzero_subnormal_code_count{};
    std::optional<long double> minimum_positive_subnormal;
    std::optional<long double> minimum_positive_normal;
    std::optional<long double> maximum_finite;
    std::optional<long double> minimum_finite;
    std::optional<long double> unit_interval_resolution;
    long double maximum_relative_rounding_error{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits;
    std::vector<Diagnostic> diagnostics;
};

struct IntegerVarintAnalysis {
    lispb::schema::TypeId type;
    lispb::schema::TypeId source_type;
    codegen::IntegerVarintEncoding encoding{codegen::IntegerVarintEncoding::unsigned_varint};
    std::uint32_t minimum_encoded_bytes{};
    std::uint32_t maximum_encoded_bytes{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> minimum_total_bytes;
    std::optional<std::uint64_t> maximum_total_bytes;
    std::vector<Diagnostic> diagnostics;
};

struct IntegerVarintComparison {
    IntegerVarintAnalysis first;
    IntegerVarintAnalysis second;
    bool compatible_source{};
    std::optional<NumericDelta> minimum_encoded_byte_delta;
    std::optional<NumericDelta> maximum_encoded_byte_delta;
    std::optional<NumericDelta> minimum_total_byte_delta;
    std::optional<NumericDelta> maximum_total_byte_delta;
    std::vector<Diagnostic> diagnostics;
};

struct IntegerVarintDistributionEntry {
    codegen::PackedIntegerValue value;
    std::uint64_t weight{};
};

struct IntegerVarintDistributionEntryAnalysis {
    codegen::PackedIntegerValue value;
    std::uint64_t weight{};
    std::uint32_t encoded_bytes{};
    std::optional<std::uint64_t> weighted_encoded_bytes;
};

struct IntegerVarintDistributionAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t valid_entry_count{};
    std::optional<std::uint64_t> total_weight;
    std::optional<std::uint64_t> total_encoded_bytes;
    std::optional<long double> expected_bytes_per_value;
    std::uint64_t selected_element_count{1};
    std::optional<long double> expected_selected_bytes;
    std::vector<IntegerVarintDistributionEntryAnalysis> entries;
    std::vector<Diagnostic> diagnostics;
};

struct IntegerVarintDistributionComparison {
    IntegerVarintDistributionAnalysis first;
    IntegerVarintDistributionAnalysis second;
    bool compatible_source{};
    std::optional<NumericDelta> total_encoded_byte_delta;
    std::optional<long double> expected_bytes_per_value_delta;
    std::optional<long double> expected_selected_bytes_delta;
    std::vector<Diagnostic> diagnostics;
};

struct PackedFieldAnalysis {
    std::string name;
    std::optional<lispb::schema::TypeId> semantic_type;
    std::string logical_type;
    codegen::PackedFieldKind kind{codegen::PackedFieldKind::unsigned_integer};
    bool reserved{};
    std::uint32_t schema_bit_width{};
    bool schema_bit_width_auto{};
    std::uint32_t bit_width{};
    bool overridden{};
    std::uint64_t least_significant_bit{};
    std::optional<std::uint64_t> most_significant_bit;
    std::optional<std::uint64_t> maximum_unsigned_value;
    std::optional<std::int64_t> minimum_signed_value;
    std::optional<std::int64_t> maximum_signed_value;
    std::optional<codegen::PackedIntegerValue> minimum_semantic_value;
    std::optional<codegen::PackedIntegerValue> maximum_semantic_value;
    std::optional<std::uint64_t> semantic_value_count;
    std::uint64_t sentinel_code_count{};
    std::optional<std::uint64_t> required_code_count;
    std::optional<std::uint32_t> minimum_required_bits;
    std::optional<std::uint64_t> unused_codes;
    std::vector<PackedNamedCodeAnalysis> named_codes;
    std::optional<LinearQuantizedAnalysis> linear_quantized;
    std::optional<FixedPointAnalysis> fixed_point;
    std::optional<codegen::SemanticRelationKind> relationship_kind;
    std::optional<codegen::SemanticRelationUnit> relationship_unit;
    std::optional<std::string> relationship_target;
    std::optional<std::uint64_t> relationship_target_extent;
    std::optional<ExactCodeCount> relationship_live_value_count;
    std::optional<ExactCodeCount> relationship_required_code_count;
    std::optional<std::uint32_t> relationship_minimum_required_bits;
    std::optional<bool> relationship_width_sufficient;
    std::optional<std::uint64_t> relationship_code_space_capacity_limit;
    std::optional<std::uint64_t> relationship_capacity_headroom;
    std::optional<std::uint64_t> relationship_semantic_capacity_limit;
    std::optional<std::uint64_t> relationship_sentinel_capacity_limit;
    std::optional<std::uint64_t> relationship_effective_capacity_limit;
    std::optional<std::uint64_t> relationship_effective_capacity_headroom;
};

struct RelationshipTargetFacts {
    lispb::schema::TypeId target;
    std::optional<std::uint64_t> element_capacity;
    std::optional<std::uint64_t> byte_extent;
};

struct PackedAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_payload_bits;
    std::optional<std::uint64_t> total_reserved_bits;
    std::optional<std::uint64_t> total_unused_bits;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> cache_line_straddling_elements;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<std::uint64_t> page_straddling_elements;
    CacheCapacityAnalysis cache_capacity;
};

struct PackedAnalysis {
    lispb::schema::TypeId type;
    std::string schema_storage_type;
    std::string storage_type;
    bool storage_overridden{};
    std::optional<codegen::PackedByteOrder> byte_order;
    codegen::PackedBitOrder bit_order{codegen::PackedBitOrder::least_significant_first};
    std::optional<TypeFacts> storage_facts;
    std::optional<std::uint64_t> storage_bits;
    std::optional<std::uint64_t> bits_used;
    std::optional<std::uint64_t> payload_bits;
    std::optional<std::uint64_t> reserved_bits;
    std::optional<std::uint64_t> unused_bits;
    std::optional<std::uint64_t> overflow_bits;
    std::optional<std::uint64_t> invalid_raw_value;
    std::vector<PackedFieldAnalysis> fields;
    std::vector<Diagnostic> diagnostics;
    PackedAggregateAnalysis aggregate;
};

struct PackedFieldTargetComparison {
    std::string name;
    PackedFieldAnalysis first;
    PackedFieldAnalysis second;
    std::optional<NumericDelta> least_significant_bit_delta;
    std::optional<NumericDelta> most_significant_bit_delta;
    std::optional<NumericDelta> unused_code_delta;
    std::optional<NumericDelta> relationship_target_extent_delta;
    std::optional<NumericDelta> relationship_minimum_required_bit_delta;
    std::optional<NumericDelta> relationship_capacity_headroom_delta;
    std::optional<NumericDelta> relationship_effective_capacity_limit_delta;
    std::optional<NumericDelta> relationship_effective_capacity_headroom_delta;
};

struct PackedTargetComparison {
    PackedAnalysis first;
    PackedAnalysis second;
    std::vector<PackedFieldTargetComparison> fields;
    std::optional<NumericDelta> storage_size_delta;
    std::optional<NumericDelta> storage_alignment_delta;
    std::optional<NumericDelta> storage_value_bit_delta;
    std::optional<NumericDelta> storage_bit_delta;
    std::optional<NumericDelta> bits_used_delta;
    std::optional<NumericDelta> payload_bit_delta;
    std::optional<NumericDelta> reserved_bit_delta;
    std::optional<NumericDelta> unused_bit_delta;
    std::optional<NumericDelta> overflow_bit_delta;
    std::optional<NumericDelta> total_storage_delta;
    std::optional<NumericDelta> total_payload_bit_delta;
    std::optional<NumericDelta> total_reserved_bit_delta;
    std::optional<NumericDelta> total_unused_bit_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> complete_elements_per_cache_line_delta;
    std::optional<NumericDelta> cache_line_straddling_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
    std::optional<NumericDelta> page_straddling_delta;
    std::vector<Diagnostic> diagnostics;
};

struct RecordMemberAnalysis {
    std::string name;
    lispb::schema::TypeId semantic_type;
    std::uint64_t element_count{1};
    std::optional<TypeFacts> element_facts;
    std::optional<std::uint64_t> offset_bytes;
    std::optional<std::uint64_t> extent_bytes;
    std::optional<std::uint64_t> padding_before_bytes;
};

struct RecordAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_payload_bytes;
    std::optional<std::uint64_t> total_internal_padding_bytes;
    std::optional<std::uint64_t> total_tail_padding_bytes;
    std::optional<std::uint64_t> total_padding_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> cache_line_straddling_elements;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<std::uint64_t> page_straddling_elements;
    CacheCapacityAnalysis cache_capacity;
};

struct RecordAnalysis {
    lispb::schema::TypeId type;
    std::vector<RecordMemberAnalysis> members;
    std::optional<std::uint64_t> payload_bytes;
    std::optional<std::uint64_t> internal_padding_bytes;
    std::optional<std::uint64_t> tail_padding_bytes;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint64_t> alignment_bytes;
    std::vector<Diagnostic> diagnostics;
    RecordAggregateAnalysis aggregate;
};

struct RecordMemberTargetComparison {
    std::string name;
    RecordMemberAnalysis first;
    RecordMemberAnalysis second;
    std::optional<NumericDelta> element_size_delta;
    std::optional<NumericDelta> element_alignment_delta;
    std::optional<NumericDelta> offset_delta;
    std::optional<NumericDelta> extent_delta;
    std::optional<NumericDelta> padding_before_delta;
};

struct RecordTargetComparison {
    RecordAnalysis first;
    RecordAnalysis second;
    std::vector<RecordMemberTargetComparison> members;
    std::optional<NumericDelta> payload_delta;
    std::optional<NumericDelta> internal_padding_delta;
    std::optional<NumericDelta> tail_padding_delta;
    std::optional<NumericDelta> size_delta;
    std::optional<NumericDelta> alignment_delta;
    std::optional<NumericDelta> total_storage_delta;
    std::optional<NumericDelta> total_payload_delta;
    std::optional<NumericDelta> total_internal_padding_delta;
    std::optional<NumericDelta> total_tail_padding_delta;
    std::optional<NumericDelta> total_padding_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> complete_elements_per_cache_line_delta;
    std::optional<NumericDelta> cache_line_straddling_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
    std::optional<NumericDelta> page_straddling_delta;
    std::vector<Diagnostic> diagnostics;
};

struct UnionAlternativeAnalysis {
    std::string name;
    lispb::schema::TypeId semantic_type;
    std::uint64_t element_count{1};
    std::optional<TypeFacts> element_facts;
    std::optional<std::uint64_t> extent_bytes;
    std::optional<std::uint64_t> slack_bytes;
    std::optional<std::uint64_t> total_slack_bytes;
};

struct UnionAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_tail_padding_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> cache_line_straddling_elements;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<std::uint64_t> page_straddling_elements;
    CacheCapacityAnalysis cache_capacity;
};

struct UnionAnalysis {
    lispb::schema::TypeId type;
    std::vector<UnionAlternativeAnalysis> alternatives;
    std::optional<std::uint64_t> largest_alternative_bytes;
    std::optional<std::uint64_t> tail_padding_bytes;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint64_t> alignment_bytes;
    std::vector<Diagnostic> diagnostics;
    UnionAggregateAnalysis aggregate;
};

struct UnionAlternativeTargetComparison {
    std::string name;
    UnionAlternativeAnalysis first;
    UnionAlternativeAnalysis second;
    std::optional<NumericDelta> element_size_delta;
    std::optional<NumericDelta> element_alignment_delta;
    std::optional<NumericDelta> extent_delta;
    std::optional<NumericDelta> slack_delta;
    std::optional<NumericDelta> total_slack_delta;
};

struct UnionTargetComparison {
    UnionAnalysis first;
    UnionAnalysis second;
    std::vector<UnionAlternativeTargetComparison> alternatives;
    std::optional<NumericDelta> largest_alternative_delta;
    std::optional<NumericDelta> tail_padding_delta;
    std::optional<NumericDelta> size_delta;
    std::optional<NumericDelta> alignment_delta;
    std::optional<NumericDelta> total_storage_delta;
    std::optional<NumericDelta> total_tail_padding_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> complete_elements_per_cache_line_delta;
    std::optional<NumericDelta> cache_line_straddling_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
    std::optional<NumericDelta> page_straddling_delta;
    std::vector<Diagnostic> diagnostics;
};

struct UnionDistributionEntry {
    std::string alternative_name;
    std::uint64_t weight{};
};

struct UnionDistributionEntryAnalysis {
    std::string alternative_name;
    std::uint64_t weight{};
    std::optional<std::uint64_t> extent_bytes;
    std::optional<std::uint64_t> slack_bytes;
    std::optional<std::uint64_t> weighted_extent_bytes;
    std::optional<std::uint64_t> weighted_slack_bytes;
};

struct UnionDistributionAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t valid_entry_count{};
    std::optional<std::uint64_t> total_weight;
    std::optional<std::uint64_t> total_extent_bytes;
    std::optional<std::uint64_t> total_slack_bytes;
    std::optional<long double> expected_extent_bytes_per_value;
    std::optional<long double> expected_slack_bytes_per_value;
    std::uint64_t selected_element_count{};
    std::optional<long double> expected_selected_extent_bytes;
    std::optional<long double> expected_selected_slack_bytes;
    std::vector<UnionDistributionEntryAnalysis> entries;
    std::vector<Diagnostic> diagnostics;
};

struct UnionDistributionEntryComparison {
    std::string alternative_name;
    UnionDistributionEntryAnalysis first;
    UnionDistributionEntryAnalysis second;
    std::optional<NumericDelta> extent_delta;
    std::optional<NumericDelta> slack_delta;
    std::optional<NumericDelta> weighted_extent_delta;
    std::optional<NumericDelta> weighted_slack_delta;
};

struct UnionDistributionComparison {
    UnionDistributionAnalysis first;
    UnionDistributionAnalysis second;
    std::vector<UnionDistributionEntryComparison> entries;
    std::optional<NumericDelta> total_weight_delta;
    std::optional<NumericDelta> total_extent_delta;
    std::optional<NumericDelta> total_slack_delta;
    std::optional<long double> expected_extent_per_value_delta;
    std::optional<long double> expected_slack_per_value_delta;
    std::optional<long double> expected_selected_extent_delta;
    std::optional<long double> expected_selected_slack_delta;
    std::vector<Diagnostic> diagnostics;
};

struct TaggedUnionAlternativeAnalysis {
    std::string name;
    std::string tag;
    lispb::schema::TypeId semantic_type;
    std::uint64_t element_count{1};
    std::optional<TypeFacts> element_facts;
    std::optional<std::uint64_t> extent_bytes;
    std::optional<std::uint64_t> payload_slack_bytes;
    std::optional<std::uint64_t> total_payload_slack_bytes;
};

struct TaggedUnionAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_discriminant_bytes;
    std::optional<std::uint64_t> total_payload_bytes;
    std::optional<std::uint64_t> total_internal_padding_bytes;
    std::optional<std::uint64_t> total_tail_padding_bytes;
    std::optional<std::uint64_t> total_padding_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> cache_line_straddling_elements;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<std::uint64_t> page_straddling_elements;
    CacheCapacityAnalysis cache_capacity;
};

struct TaggedUnionAnalysis {
    lispb::schema::TypeId type;
    lispb::schema::TypeId discriminant_type;
    std::optional<TypeFacts> discriminant_facts;
    std::vector<TaggedUnionAlternativeAnalysis> alternatives;
    std::optional<std::uint64_t> largest_alternative_bytes;
    std::optional<std::uint64_t> payload_size_bytes;
    std::optional<std::uint64_t> payload_alignment_bytes;
    std::optional<std::uint64_t> payload_offset_bytes;
    std::optional<std::uint64_t> internal_padding_bytes;
    std::optional<std::uint64_t> tail_padding_bytes;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint64_t> alignment_bytes;
    std::vector<std::string> mapped_live_tags;
    std::vector<std::string> unmapped_live_tags;
    std::vector<std::string> sentinel_tags;
    std::optional<std::string> count_sentinel_tag;
    std::vector<Diagnostic> diagnostics;
    TaggedUnionAggregateAnalysis aggregate;
};

struct TaggedUnionAlternativeTargetComparison {
    std::string name;
    TaggedUnionAlternativeAnalysis first;
    TaggedUnionAlternativeAnalysis second;
    std::optional<NumericDelta> element_size_delta;
    std::optional<NumericDelta> element_alignment_delta;
    std::optional<NumericDelta> extent_delta;
    std::optional<NumericDelta> payload_slack_delta;
    std::optional<NumericDelta> total_payload_slack_delta;
};

struct TaggedUnionTargetComparison {
    TaggedUnionAnalysis first;
    TaggedUnionAnalysis second;
    std::vector<TaggedUnionAlternativeTargetComparison> alternatives;
    std::optional<NumericDelta> discriminant_size_delta;
    std::optional<NumericDelta> discriminant_alignment_delta;
    std::optional<NumericDelta> largest_alternative_delta;
    std::optional<NumericDelta> payload_size_delta;
    std::optional<NumericDelta> payload_alignment_delta;
    std::optional<NumericDelta> payload_offset_delta;
    std::optional<NumericDelta> internal_padding_delta;
    std::optional<NumericDelta> tail_padding_delta;
    std::optional<NumericDelta> size_delta;
    std::optional<NumericDelta> alignment_delta;
    std::optional<NumericDelta> total_storage_delta;
    std::optional<NumericDelta> total_discriminant_delta;
    std::optional<NumericDelta> total_payload_delta;
    std::optional<NumericDelta> total_internal_padding_delta;
    std::optional<NumericDelta> total_tail_padding_delta;
    std::optional<NumericDelta> total_padding_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> complete_elements_per_cache_line_delta;
    std::optional<NumericDelta> cache_line_straddling_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
    std::optional<NumericDelta> page_straddling_delta;
    std::vector<Diagnostic> diagnostics;
};

struct TaggedUnionDistributionEntry {
    std::string tag;
    std::uint64_t weight{};
};

struct TaggedUnionDistributionEntryAnalysis {
    std::string tag;
    std::string alternative_name;
    std::uint64_t weight{};
    std::optional<std::uint64_t> payload_extent_bytes;
    std::optional<std::uint64_t> payload_slack_bytes;
    std::optional<std::uint64_t> weighted_payload_extent_bytes;
    std::optional<std::uint64_t> weighted_payload_slack_bytes;
};

struct TaggedUnionDistributionAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t valid_entry_count{};
    std::optional<std::uint64_t> total_weight;
    std::optional<std::uint64_t> total_payload_extent_bytes;
    std::optional<std::uint64_t> total_payload_slack_bytes;
    std::optional<long double> expected_payload_extent_bytes_per_value;
    std::optional<long double> expected_payload_slack_bytes_per_value;
    std::uint64_t selected_element_count{1};
    std::optional<long double> expected_selected_payload_extent_bytes;
    std::optional<long double> expected_selected_payload_slack_bytes;
    std::vector<TaggedUnionDistributionEntryAnalysis> entries;
    std::vector<Diagnostic> diagnostics;
};

struct TaggedUnionDistributionEntryComparison {
    std::string tag;
    TaggedUnionDistributionEntryAnalysis first;
    TaggedUnionDistributionEntryAnalysis second;
    std::optional<NumericDelta> payload_extent_delta;
    std::optional<NumericDelta> payload_slack_delta;
    std::optional<NumericDelta> weighted_payload_extent_delta;
    std::optional<NumericDelta> weighted_payload_slack_delta;
};

struct TaggedUnionDistributionComparison {
    TaggedUnionDistributionAnalysis first;
    TaggedUnionDistributionAnalysis second;
    std::vector<TaggedUnionDistributionEntryComparison> entries;
    std::optional<NumericDelta> total_weight_delta;
    std::optional<NumericDelta> total_payload_extent_delta;
    std::optional<NumericDelta> total_payload_slack_delta;
    std::optional<long double> expected_payload_extent_per_value_delta;
    std::optional<long double> expected_payload_slack_per_value_delta;
    std::optional<long double> expected_selected_payload_extent_delta;
    std::optional<long double> expected_selected_payload_slack_delta;
    std::vector<Diagnostic> diagnostics;
};

enum class AccessOperation : std::uint8_t {
    read,
    write,
    read_write,
};

struct AccessIntent {
    std::string name;
    AccessOperation operation{AccessOperation::read};

    auto operator==(AccessIntent const&) const -> bool = default;
};

struct PackedFieldAccessAnalysis {
    std::string name;
    AccessOperation operation{AccessOperation::read};
    std::uint32_t bit_width{};
    std::optional<std::uint64_t> useful_bits;
    std::optional<std::uint64_t> read_useful_bits;
    std::optional<std::uint64_t> write_useful_bits;
    std::optional<std::uint64_t> logical_read_useful_bits;
    std::optional<std::uint64_t> logical_write_useful_bits;
};

struct PackedAccessAnalysis {
    lispb::schema::TypeId type;
    std::vector<std::string> field_names;
    std::vector<AccessIntent> accesses;
    std::vector<PackedFieldAccessAnalysis> fields;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    std::optional<std::uint64_t> useful_bits;
    std::optional<std::uint64_t> read_useful_bits;
    std::optional<std::uint64_t> write_useful_bits;
    std::optional<std::uint64_t> logical_read_useful_bits;
    std::optional<std::uint64_t> logical_write_useful_bits;
    std::optional<std::uint64_t> storage_footprint_bytes;
    std::optional<std::uint64_t> storage_footprint_bits;
    std::optional<std::uint64_t> non_useful_storage_bits;
    std::optional<std::uint64_t> unselected_field_bits;
    std::optional<std::uint64_t> reserved_region_bits;
    std::optional<std::uint64_t> physically_unused_storage_bits;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines_touched;
    std::optional<std::uint64_t> minimum_cache_bytes_touched;
    std::optional<std::uint64_t> read_cache_lines_touched;
    std::optional<std::uint64_t> read_cache_bytes_touched;
    std::optional<std::uint64_t> write_cache_lines_touched;
    std::optional<std::uint64_t> write_cache_bytes_touched;
    CacheCapacityAnalysis cache_footprint_capacity;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages_touched;
    std::optional<std::uint64_t> minimum_page_bytes_touched;
    std::optional<std::uint64_t> read_pages_touched;
    std::optional<std::uint64_t> read_page_bytes_touched;
    std::optional<std::uint64_t> write_pages_touched;
    std::optional<std::uint64_t> write_page_bytes_touched;
    std::vector<Diagnostic> diagnostics;
};

struct PackedFieldAccessComparison {
    std::string name;
    PackedFieldAccessAnalysis first;
    PackedFieldAccessAnalysis second;
    std::optional<NumericDelta> bit_width_delta;
    std::optional<NumericDelta> useful_bit_delta;
    std::optional<NumericDelta> read_useful_bit_delta;
    std::optional<NumericDelta> write_useful_bit_delta;
    std::optional<NumericDelta> logical_read_useful_bit_delta;
    std::optional<NumericDelta> logical_write_useful_bit_delta;
};

struct PackedAccessComparison {
    std::vector<std::string> field_names;
    std::vector<AccessIntent> accesses;
    std::vector<PackedFieldAccessComparison> fields;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    PackedAccessAnalysis first;
    PackedAccessAnalysis second;
    std::optional<NumericDelta> useful_bit_delta;
    std::optional<NumericDelta> read_useful_bit_delta;
    std::optional<NumericDelta> write_useful_bit_delta;
    std::optional<NumericDelta> logical_read_useful_bit_delta;
    std::optional<NumericDelta> logical_write_useful_bit_delta;
    std::optional<NumericDelta> storage_footprint_byte_delta;
    std::optional<NumericDelta> storage_footprint_bit_delta;
    std::optional<NumericDelta> non_useful_storage_bit_delta;
    std::optional<NumericDelta> unselected_field_bit_delta;
    std::optional<NumericDelta> reserved_region_bit_delta;
    std::optional<NumericDelta> physically_unused_storage_bit_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> cache_line_delta;
    std::optional<NumericDelta> cache_byte_delta;
    std::optional<NumericDelta> read_cache_line_delta;
    std::optional<NumericDelta> read_cache_byte_delta;
    std::optional<NumericDelta> write_cache_line_delta;
    std::optional<NumericDelta> write_cache_byte_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> page_delta;
    std::optional<NumericDelta> page_byte_delta;
    std::optional<NumericDelta> read_page_delta;
    std::optional<NumericDelta> read_page_byte_delta;
    std::optional<NumericDelta> write_page_delta;
    std::optional<NumericDelta> write_page_byte_delta;
    std::vector<Diagnostic> diagnostics;
};

struct RecordAccessAnalysis {
    lispb::schema::TypeId type;
    std::vector<std::string> member_names;
    std::vector<AccessIntent> accesses;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> read_useful_bytes;
    std::optional<std::uint64_t> write_useful_bytes;
    std::optional<std::uint64_t> logical_read_useful_bytes;
    std::optional<std::uint64_t> logical_write_useful_bytes;
    std::optional<std::uint64_t> object_footprint_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> cache_lines_touched;
    std::optional<std::uint64_t> cache_bytes_touched;
    std::optional<std::uint64_t> read_cache_lines_touched;
    std::optional<std::uint64_t> read_cache_bytes_touched;
    std::optional<std::uint64_t> write_cache_lines_touched;
    std::optional<std::uint64_t> write_cache_bytes_touched;
    std::optional<std::uint64_t> non_selected_cache_bytes;
    CacheCapacityAnalysis cache_footprint_capacity;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> pages_touched;
    std::optional<std::uint64_t> page_bytes_touched;
    std::optional<std::uint64_t> read_pages_touched;
    std::optional<std::uint64_t> read_page_bytes_touched;
    std::optional<std::uint64_t> write_pages_touched;
    std::optional<std::uint64_t> write_page_bytes_touched;
    std::optional<std::uint64_t> non_selected_page_bytes;
    std::vector<Diagnostic> diagnostics;
};

struct RecordAccessComparison {
    RecordAccessAnalysis first;
    RecordAccessAnalysis second;
    std::optional<NumericDelta> useful_byte_delta;
    std::optional<NumericDelta> read_useful_byte_delta;
    std::optional<NumericDelta> write_useful_byte_delta;
    std::optional<NumericDelta> logical_read_useful_byte_delta;
    std::optional<NumericDelta> logical_write_useful_byte_delta;
    std::optional<NumericDelta> object_footprint_byte_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> cache_line_delta;
    std::optional<NumericDelta> cache_byte_delta;
    std::optional<NumericDelta> read_cache_line_delta;
    std::optional<NumericDelta> read_cache_byte_delta;
    std::optional<NumericDelta> write_cache_line_delta;
    std::optional<NumericDelta> write_cache_byte_delta;
    std::optional<NumericDelta> non_selected_cache_byte_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> page_delta;
    std::optional<NumericDelta> page_byte_delta;
    std::optional<NumericDelta> read_page_delta;
    std::optional<NumericDelta> read_page_byte_delta;
    std::optional<NumericDelta> write_page_delta;
    std::optional<NumericDelta> write_page_byte_delta;
    std::optional<NumericDelta> non_selected_page_byte_delta;
    std::vector<Diagnostic> diagnostics;
};

struct CacheLineTiling {
    std::uint64_t cache_line_bytes{};
    std::uint64_t element_bytes{};
    std::optional<std::uint64_t> exact_elements_per_cache_line;
    std::uint64_t complete_elements_from_line_start{};
    std::uint64_t boundary_fragment_bytes{};
    std::uint64_t minimum_cache_lines_per_element{};
};

struct SoaColumnAnalysis {
    std::string name;
    lispb::schema::TypeId semantic_type;
    std::string schema_type;
    std::string physical_type;
    bool overridden{};
    std::optional<TypeFacts> type_facts;
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> allocation_offset_bytes;
    std::optional<std::uint64_t> padding_before_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> elements_per_cache_line;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<CacheLineTiling> cache_line_tiling;
};

enum class SoaAllocationStrategy : std::uint8_t {
    separate_columns,
    aligned_contiguous,
};

struct SoaAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t capacity{};
    bool capacity_overridden{};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    std::uint64_t allocation_count{};
    std::vector<SoaColumnAnalysis> columns;
    std::optional<std::uint64_t> bytes_per_logical_element;
    std::optional<std::uint64_t> total_payload_bytes;
    std::optional<std::uint64_t> total_allocation_bytes;
    std::optional<std::uint64_t> total_alignment_padding_bytes;
    std::optional<std::uint64_t> allocation_alignment_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    CacheCapacityAnalysis cache_capacity;
    std::vector<Diagnostic> diagnostics;
};

struct SoaColumnTargetComparison {
    std::string name;
    SoaColumnAnalysis first;
    SoaColumnAnalysis second;
    std::optional<NumericDelta> element_size_delta;
    std::optional<NumericDelta> element_alignment_delta;
    std::optional<NumericDelta> total_byte_delta;
    std::optional<NumericDelta> allocation_offset_delta;
    std::optional<NumericDelta> padding_before_delta;
    std::optional<NumericDelta> minimum_cache_line_delta;
    std::optional<NumericDelta> elements_per_cache_line_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::optional<NumericDelta> complete_elements_per_page_delta;
};

struct SoaTargetComparison {
    bool compatible{};
    SoaAnalysis first;
    SoaAnalysis second;
    std::vector<SoaColumnTargetComparison> columns;
    std::optional<NumericDelta> allocation_count_delta;
    std::optional<NumericDelta> bytes_per_logical_element_delta;
    std::optional<NumericDelta> total_payload_delta;
    std::optional<NumericDelta> total_allocation_delta;
    std::optional<NumericDelta> total_alignment_padding_delta;
    std::optional<NumericDelta> allocation_alignment_delta;
    std::optional<NumericDelta> cache_line_size_delta;
    std::optional<NumericDelta> page_size_delta;
    std::optional<NumericDelta> minimum_page_delta;
    std::vector<Diagnostic> diagnostics;
};

struct SoaColumnAccessAnalysis {
    std::string name;
    AccessOperation operation{AccessOperation::read};
    std::string physical_type;
    std::optional<std::uint64_t> element_bytes;
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> read_useful_bytes;
    std::optional<std::uint64_t> write_useful_bytes;
    std::optional<std::uint64_t> logical_read_useful_bytes;
    std::optional<std::uint64_t> logical_write_useful_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> minimum_cache_bytes;
    std::optional<std::uint64_t> non_payload_cache_bytes;
    std::optional<std::uint64_t> aligned_cache_line_straddling_elements;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> minimum_page_bytes;
    std::optional<std::uint64_t> non_payload_page_bytes;
    std::optional<std::uint64_t> aligned_page_straddling_elements;
    std::optional<std::uint64_t> allocated_capacity_payload_bytes;
    std::optional<std::uint64_t> capacity_slack_payload_bytes;
};

struct SoaAccessAnalysis {
    std::vector<std::string> column_names;
    std::vector<AccessIntent> accesses;
    std::vector<SoaColumnAccessAnalysis> columns;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool footprint_exact{};
    std::uint64_t allocation_count{};
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> read_useful_bytes;
    std::optional<std::uint64_t> write_useful_bytes;
    std::optional<std::uint64_t> logical_read_useful_bytes;
    std::optional<std::uint64_t> logical_write_useful_bytes;
    std::optional<std::uint64_t> full_logical_payload_bytes;
    std::optional<std::uint64_t> unselected_payload_bytes;
    std::optional<std::uint64_t> allocated_capacity_payload_bytes;
    std::optional<std::uint64_t> capacity_slack_payload_bytes;
    std::optional<std::uint64_t> total_allocation_bytes;
    std::optional<std::uint64_t> alignment_padding_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines_touched;
    std::optional<std::uint64_t> minimum_cache_bytes_touched;
    std::optional<std::uint64_t> minimum_read_cache_lines_touched;
    std::optional<std::uint64_t> minimum_read_cache_bytes_touched;
    std::optional<std::uint64_t> minimum_write_cache_lines_touched;
    std::optional<std::uint64_t> minimum_write_cache_bytes_touched;
    std::optional<std::uint64_t> non_payload_cache_bytes;
    CacheCapacityAnalysis minimum_cache_footprint_capacity;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages_touched;
    std::optional<std::uint64_t> minimum_page_bytes_touched;
    std::optional<std::uint64_t> minimum_read_pages_touched;
    std::optional<std::uint64_t> minimum_read_page_bytes_touched;
    std::optional<std::uint64_t> minimum_write_pages_touched;
    std::optional<std::uint64_t> minimum_write_page_bytes_touched;
    std::optional<std::uint64_t> non_payload_page_bytes;
    std::vector<Diagnostic> diagnostics;
};

struct AccessFootprintSummary {
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> read_useful_bytes;
    std::optional<std::uint64_t> write_useful_bytes;
    std::optional<std::uint64_t> logical_read_useful_bytes;
    std::optional<std::uint64_t> logical_write_useful_bytes;
    std::optional<std::uint64_t> cache_lines;
    std::optional<std::uint64_t> cache_bytes;
    std::optional<std::uint64_t> read_cache_lines;
    std::optional<std::uint64_t> read_cache_bytes;
    std::optional<std::uint64_t> write_cache_lines;
    std::optional<std::uint64_t> write_cache_bytes;
    std::optional<std::uint64_t> pages;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> read_pages;
    std::optional<std::uint64_t> read_page_bytes;
    std::optional<std::uint64_t> write_pages;
    std::optional<std::uint64_t> write_page_bytes;
};

struct RecordSoaAccessComparison {
    std::vector<std::string> member_names;
    std::vector<AccessIntent> accesses;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy soa_allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool soa_footprint_exact{};
    AccessFootprintSummary record;
    AccessFootprintSummary soa;
    CacheCapacityAnalysis record_cache_footprint_capacity;
    CacheCapacityAnalysis soa_minimum_cache_footprint_capacity;
    std::optional<std::uint64_t> record_non_useful_cache_bytes;
    std::optional<std::uint64_t> soa_non_useful_cache_bytes;
    std::optional<std::uint64_t> record_non_useful_page_bytes;
    std::optional<std::uint64_t> soa_non_useful_page_bytes;
    std::optional<NumericDelta> useful_byte_delta;
    std::optional<NumericDelta> read_useful_byte_delta;
    std::optional<NumericDelta> write_useful_byte_delta;
    std::optional<NumericDelta> logical_read_useful_byte_delta;
    std::optional<NumericDelta> logical_write_useful_byte_delta;
    std::optional<NumericDelta> cache_line_delta;
    std::optional<NumericDelta> cache_byte_delta;
    std::optional<NumericDelta> read_cache_line_delta;
    std::optional<NumericDelta> read_cache_byte_delta;
    std::optional<NumericDelta> write_cache_line_delta;
    std::optional<NumericDelta> write_cache_byte_delta;
    std::optional<NumericDelta> page_delta;
    std::optional<NumericDelta> page_byte_delta;
    std::optional<NumericDelta> read_page_delta;
    std::optional<NumericDelta> read_page_byte_delta;
    std::optional<NumericDelta> write_page_delta;
    std::optional<NumericDelta> write_page_byte_delta;
    std::optional<NumericDelta> non_useful_cache_byte_delta;
    std::optional<NumericDelta> non_useful_page_byte_delta;
    std::vector<Diagnostic> diagnostics;
};

struct SoaColumnAccessComparison {
    std::string name;
    SoaColumnAccessAnalysis first;
    SoaColumnAccessAnalysis second;
    std::optional<NumericDelta> element_byte_delta;
    std::optional<NumericDelta> useful_byte_delta;
    std::optional<NumericDelta> read_useful_byte_delta;
    std::optional<NumericDelta> write_useful_byte_delta;
    std::optional<NumericDelta> logical_read_useful_byte_delta;
    std::optional<NumericDelta> logical_write_useful_byte_delta;
    std::optional<NumericDelta> cache_line_delta;
    std::optional<NumericDelta> cache_byte_delta;
    std::optional<NumericDelta> non_payload_cache_byte_delta;
    std::optional<NumericDelta> aligned_cache_line_straddling_element_delta;
    std::optional<NumericDelta> page_delta;
    std::optional<NumericDelta> page_byte_delta;
    std::optional<NumericDelta> non_payload_page_byte_delta;
    std::optional<NumericDelta> aligned_page_straddling_element_delta;
    std::optional<NumericDelta> allocated_capacity_payload_delta;
    std::optional<NumericDelta> capacity_slack_payload_delta;
};

struct SoaAccessComparison {
    std::vector<std::string> column_names;
    std::vector<AccessIntent> accesses;
    std::vector<SoaColumnAccessComparison> columns;
    std::uint64_t element_count{};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    bool footprint_exact{};
    AccessFootprintSummary first;
    AccessFootprintSummary second;
    std::uint64_t first_allocation_count{};
    std::uint64_t second_allocation_count{};
    std::optional<std::uint64_t> first_total_allocation_bytes;
    std::optional<std::uint64_t> second_total_allocation_bytes;
    std::optional<std::uint64_t> first_alignment_padding_bytes;
    std::optional<std::uint64_t> second_alignment_padding_bytes;
    std::optional<NumericDelta> allocation_count_delta;
    std::optional<NumericDelta> total_allocation_byte_delta;
    std::optional<NumericDelta> alignment_padding_byte_delta;
    CacheCapacityAnalysis first_minimum_cache_footprint_capacity;
    CacheCapacityAnalysis second_minimum_cache_footprint_capacity;
    std::optional<std::uint64_t> first_allocated_capacity_payload_bytes;
    std::optional<std::uint64_t> second_allocated_capacity_payload_bytes;
    std::optional<std::uint64_t> first_capacity_slack_payload_bytes;
    std::optional<std::uint64_t> second_capacity_slack_payload_bytes;
    std::optional<std::uint64_t> first_full_logical_payload_bytes;
    std::optional<std::uint64_t> second_full_logical_payload_bytes;
    std::optional<std::uint64_t> first_unselected_payload_bytes;
    std::optional<std::uint64_t> second_unselected_payload_bytes;
    std::optional<std::uint64_t> first_non_payload_cache_bytes;
    std::optional<std::uint64_t> second_non_payload_cache_bytes;
    std::optional<std::uint64_t> first_non_payload_page_bytes;
    std::optional<std::uint64_t> second_non_payload_page_bytes;
    std::optional<NumericDelta> useful_byte_delta;
    std::optional<NumericDelta> read_useful_byte_delta;
    std::optional<NumericDelta> write_useful_byte_delta;
    std::optional<NumericDelta> logical_read_useful_byte_delta;
    std::optional<NumericDelta> logical_write_useful_byte_delta;
    std::optional<NumericDelta> cache_line_delta;
    std::optional<NumericDelta> cache_byte_delta;
    std::optional<NumericDelta> read_cache_line_delta;
    std::optional<NumericDelta> read_cache_byte_delta;
    std::optional<NumericDelta> write_cache_line_delta;
    std::optional<NumericDelta> write_cache_byte_delta;
    std::optional<NumericDelta> page_delta;
    std::optional<NumericDelta> page_byte_delta;
    std::optional<NumericDelta> read_page_delta;
    std::optional<NumericDelta> read_page_byte_delta;
    std::optional<NumericDelta> write_page_delta;
    std::optional<NumericDelta> write_page_byte_delta;
    std::optional<NumericDelta> allocated_capacity_payload_delta;
    std::optional<NumericDelta> capacity_slack_payload_delta;
    std::optional<NumericDelta> full_logical_payload_delta;
    std::optional<NumericDelta> unselected_payload_delta;
    std::optional<NumericDelta> non_payload_cache_byte_delta;
    std::optional<NumericDelta> non_payload_page_byte_delta;
    std::vector<Diagnostic> diagnostics;
};

class Analyzer {
  public:
    static auto analyze_enum(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId type,
                             AbiProfile const& abi,
                             std::uint64_t element_count = 1) -> EnumDomainAnalysis;
    static auto compare_enum_targets(EnumDomainAnalysis const& first,
                                     EnumDomainAnalysis const& second) -> EnumTargetComparison;
    static auto
        analyze_integer_scalar(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               std::span<RelationshipTargetFacts const> relationship_targets = {})
            -> IntegerScalarAnalysis;
    static auto compare_integer_scalar_capacity(
        lispb::schema::TypeGraph const& types,
        lispb::schema::TypeId type,
        std::span<RelationshipTargetFacts const> first_relationship_targets,
        std::span<RelationshipTargetFacts const> second_relationship_targets)
        -> IntegerScalarCapacityComparison;
    static auto analyze_optional_sentinel(lispb::schema::TypeGraph const& types,
                                          lispb::schema::TypeId type,
                                          std::uint64_t element_count = 1)
        -> OptionalSentinelAnalysis;
    static auto analyze_optional_presence_bit(lispb::schema::TypeGraph const& types,
                                              lispb::schema::TypeId type,
                                              std::uint64_t element_count = 1)
        -> OptionalPresenceBitAnalysis;
    static auto compare_optional_encodings(lispb::schema::TypeGraph const& types,
                                           lispb::schema::TypeId first,
                                           lispb::schema::TypeId second,
                                           std::uint64_t element_count = 1)
        -> OptionalEncodingComparison;
    static auto analyze_linear_quantized(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId type) -> LinearQuantizedAnalysis;
    static auto compare_linear_quantized(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId first,
                                         lispb::schema::TypeId second,
                                         std::uint64_t element_count = 1)
        -> LinearQuantizedComparison;
    static auto analyze_fixed_point(lispb::schema::TypeGraph const& types,
                                    lispb::schema::TypeId type,
                                    std::uint64_t element_count = 1) -> FixedPointAnalysis;
    static auto analyze_mini_float(lispb::schema::TypeGraph const& types,
                                   lispb::schema::TypeId type,
                                   std::uint64_t element_count = 1) -> MiniFloatAnalysis;
    static auto analyze_integer_varint(lispb::schema::TypeGraph const& types,
                                       lispb::schema::TypeId type,
                                       std::uint64_t element_count = 1) -> IntegerVarintAnalysis;
    static auto compare_integer_varint(lispb::schema::TypeGraph const& types,
                                       lispb::schema::TypeId first,
                                       lispb::schema::TypeId second,
                                       std::uint64_t element_count = 1) -> IntegerVarintComparison;
    static auto
        analyze_integer_varint_distribution(lispb::schema::TypeGraph const& types,
                                            lispb::schema::TypeId type,
                                            std::span<IntegerVarintDistributionEntry const> entries,
                                            std::uint64_t selected_element_count = 1)
            -> IntegerVarintDistributionAnalysis;
    static auto
        compare_integer_varint_distribution(lispb::schema::TypeGraph const& types,
                                            lispb::schema::TypeId first,
                                            lispb::schema::TypeId second,
                                            std::span<IntegerVarintDistributionEntry const> entries,
                                            std::uint64_t selected_element_count = 1)
            -> IntegerVarintDistributionComparison;
    static auto analyze_packed(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               Variant const& variant,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1,
                               std::span<RelationshipTargetFacts const> relationship_targets = {})
        -> PackedAnalysis;
    static auto compare_packed_targets(PackedAnalysis const& first, PackedAnalysis const& second)
        -> PackedTargetComparison;
    static auto analyze_packed_access(PackedAnalysis const& packed,
                                      std::span<AccessIntent const> accesses,
                                      AbiProfile const& abi,
                                      std::uint64_t multiplicity = 1) -> PackedAccessAnalysis;
    static auto compare_packed_access(PackedAccessAnalysis const& first,
                                      PackedAccessAnalysis const& second) -> PackedAccessComparison;
    static auto analyze_record(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1) -> RecordAnalysis;
    static auto compare_record_targets(RecordAnalysis const& first, RecordAnalysis const& second)
        -> RecordTargetComparison;
    static auto analyze_union(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId type,
                              AbiProfile const& abi,
                              std::uint64_t element_count = 1) -> UnionAnalysis;
    static auto compare_union_targets(UnionAnalysis const& first, UnionAnalysis const& second)
        -> UnionTargetComparison;
    static auto analyze_union_distribution(UnionAnalysis const& analysis,
                                           std::span<UnionDistributionEntry const> entries,
                                           std::uint64_t selected_element_count)
        -> UnionDistributionAnalysis;
    static auto compare_union_distributions(UnionDistributionAnalysis const& first,
                                            UnionDistributionAnalysis const& second)
        -> UnionDistributionComparison;
    static auto analyze_tagged_union(lispb::schema::TypeGraph const& types,
                                     lispb::schema::TypeId type,
                                     AbiProfile const& abi,
                                     std::uint64_t element_count = 1) -> TaggedUnionAnalysis;
    static auto compare_tagged_union_targets(TaggedUnionAnalysis const& first,
                                             TaggedUnionAnalysis const& second)
        -> TaggedUnionTargetComparison;
    static auto
        analyze_tagged_union_distribution(TaggedUnionAnalysis const& tagged_union,
                                          std::span<TaggedUnionDistributionEntry const> entries,
                                          std::uint64_t selected_element_count)
            -> TaggedUnionDistributionAnalysis;
    static auto compare_tagged_union_distributions(TaggedUnionDistributionAnalysis const& first,
                                                   TaggedUnionDistributionAnalysis const& second)
        -> TaggedUnionDistributionComparison;
    static auto analyze_record_access(RecordAnalysis const& record,
                                      std::span<AccessIntent const> accesses,
                                      AbiProfile const& abi,
                                      std::uint64_t multiplicity = 1) -> RecordAccessAnalysis;
    static auto analyze_record_access(RecordAnalysis const& record,
                                      std::span<std::string const> member_names,
                                      AbiProfile const& abi,
                                      AccessOperation operation = AccessOperation::read,
                                      std::uint64_t multiplicity = 1) -> RecordAccessAnalysis;
    static auto analyze_record_member_access(RecordAnalysis const& record,
                                             std::string_view member_name,
                                             AbiProfile const& abi,
                                             AccessOperation operation = AccessOperation::read,
                                             std::uint64_t multiplicity = 1)
        -> RecordAccessAnalysis;
    static auto compare_record_access(RecordAccessAnalysis const& first,
                                      RecordAccessAnalysis const& second) -> RecordAccessComparison;
    static auto analyze_soa(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId type,
                            Variant const& variant,
                            AbiProfile const& abi,
                            std::uint64_t default_capacity,
                            SoaAllocationStrategy allocation_strategy =
                                SoaAllocationStrategy::separate_columns) -> SoaAnalysis;
    static auto compare_soa_targets(SoaAnalysis const& first, SoaAnalysis const& second)
        -> SoaTargetComparison;
    static auto derive_relationship_target_facts(
        lispb::schema::TypeGraph const& types,
        Variant const& variant,
        AbiProfile const& abi,
        std::uint64_t default_capacity,
        SoaAllocationStrategy allocation_strategy = SoaAllocationStrategy::separate_columns)
        -> std::vector<RelationshipTargetFacts>;
    static auto analyze_soa_access(SoaAnalysis const& soa,
                                   std::span<AccessIntent const> accesses,
                                   AbiProfile const& abi,
                                   std::uint64_t element_count,
                                   std::uint64_t multiplicity = 1) -> SoaAccessAnalysis;
    static auto analyze_soa_access(SoaAnalysis const& soa,
                                   std::span<std::string const> column_names,
                                   AbiProfile const& abi,
                                   std::uint64_t element_count,
                                   AccessOperation operation = AccessOperation::read,
                                   std::uint64_t multiplicity = 1) -> SoaAccessAnalysis;
    static auto compare_record_soa_access(RecordAccessAnalysis const& record,
                                          SoaAccessAnalysis const& soa)
        -> RecordSoaAccessComparison;
    static auto compare_soa_access(SoaAccessAnalysis const& first, SoaAccessAnalysis const& second)
        -> SoaAccessComparison;
};

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> std::optional<std::string>;

} // namespace ioj::layout
