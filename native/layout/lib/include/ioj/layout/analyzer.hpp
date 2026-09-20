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

auto numeric_delta(std::optional<std::uint64_t> baseline, std::optional<std::uint64_t> variant)
    -> std::optional<NumericDelta>;

using EnumCodeValue = lispb::schema::EnumCode;

struct EnumeratorAnalysis {
    std::string name;
    bool sentinel{};
    bool count_sentinel{};
    std::optional<EnumCodeValue> code;
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
};

struct PackedNamedCodeAnalysis {
    std::string name;
    codegen::PackedIntegerValue value;
    bool sentinel{};
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
};

struct ExactCodeCount {
    std::uint64_t value{};
    bool two_to_64{};

    auto operator==(ExactCodeCount const&) const -> bool = default;
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
    std::optional<codegen::PackedFieldRelationKind> relationship_kind;
    std::optional<std::string> relationship_target;
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

struct PackedAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_payload_bits;
    std::optional<std::uint64_t> total_reserved_bits;
    std::optional<std::uint64_t> total_unused_bits;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
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

struct RecordAccessAnalysis {
    std::vector<std::string> member_names;
    std::uint64_t element_count{};
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> object_footprint_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> cache_lines_touched;
    std::optional<std::uint64_t> cache_bytes_touched;
    std::optional<std::uint64_t> non_selected_cache_bytes;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> pages_touched;
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
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> elements_per_cache_line;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
    std::optional<CacheLineTiling> cache_line_tiling;
};

struct SoaAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t capacity{};
    bool capacity_overridden{};
    std::vector<SoaColumnAnalysis> columns;
    std::optional<std::uint64_t> bytes_per_logical_element;
    std::optional<std::uint64_t> total_payload_bytes;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    CacheCapacityAnalysis cache_capacity;
    std::vector<Diagnostic> diagnostics;
};

struct SoaAccessAnalysis {
    std::vector<std::string> column_names;
    std::uint64_t element_count{};
    std::optional<std::uint64_t> useful_bytes;
    std::optional<std::uint64_t> full_logical_payload_bytes;
    std::optional<std::uint64_t> unselected_payload_bytes;
    std::optional<std::uint64_t> allocated_capacity_payload_bytes;
    std::optional<std::uint64_t> capacity_slack_payload_bytes;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines_touched;
    std::optional<std::uint64_t> minimum_cache_bytes_touched;
    std::optional<std::uint64_t> non_payload_cache_bytes;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages_touched;
    std::optional<std::uint64_t> minimum_page_bytes_touched;
    std::optional<std::uint64_t> non_payload_page_bytes;
    std::vector<Diagnostic> diagnostics;
};

class Analyzer {
  public:
    static auto analyze_enum(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId type,
                             AbiProfile const& abi) -> EnumDomainAnalysis;
    static auto analyze_integer_scalar(lispb::schema::TypeGraph const& types,
                                       lispb::schema::TypeId type) -> IntegerScalarAnalysis;
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
                               std::uint64_t element_count = 1) -> PackedAnalysis;
    static auto analyze_record(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1) -> RecordAnalysis;
    static auto analyze_union(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId type,
                              AbiProfile const& abi,
                              std::uint64_t element_count = 1) -> UnionAnalysis;
    static auto analyze_tagged_union(lispb::schema::TypeGraph const& types,
                                     lispb::schema::TypeId type,
                                     AbiProfile const& abi,
                                     std::uint64_t element_count = 1) -> TaggedUnionAnalysis;
    static auto
        analyze_tagged_union_distribution(TaggedUnionAnalysis const& tagged_union,
                                          std::span<TaggedUnionDistributionEntry const> entries,
                                          std::uint64_t selected_element_count)
            -> TaggedUnionDistributionAnalysis;
    static auto analyze_record_access(RecordAnalysis const& record,
                                      std::span<std::string const> member_names,
                                      AbiProfile const& abi) -> RecordAccessAnalysis;
    static auto analyze_record_member_access(RecordAnalysis const& record,
                                             std::string_view member_name,
                                             AbiProfile const& abi) -> RecordAccessAnalysis;
    static auto analyze_soa(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId type,
                            Variant const& variant,
                            AbiProfile const& abi,
                            std::uint64_t default_capacity) -> SoaAnalysis;
    static auto analyze_soa_access(SoaAnalysis const& soa,
                                   std::span<std::string const> column_names,
                                   AbiProfile const& abi,
                                   std::uint64_t element_count) -> SoaAccessAnalysis;
};

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> std::optional<std::string>;

} // namespace ioj::layout
