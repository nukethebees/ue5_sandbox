#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <optional>
#include <string>
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

struct EnumCodeValue {
    bool negative{};
    std::uint64_t magnitude{};

    auto operator==(EnumCodeValue const&) const -> bool = default;
};

auto format_enum_code(EnumCodeValue value) -> std::string;

struct EnumeratorAnalysis {
    std::string name;
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
    std::optional<std::uint32_t> minimum_required_bits;
    std::optional<bool> backing_can_represent_domain;
    std::optional<std::uint64_t> unused_backing_codes;
    std::vector<EnumeratorAnalysis> enumerators;
    std::vector<Diagnostic> diagnostics;
};

struct PackedFieldAnalysis {
    std::string name;
    lispb::schema::TypeId semantic_type;
    std::string logical_type;
    codegen::PackedFieldKind kind{codegen::PackedFieldKind::unsigned_integer};
    std::uint32_t schema_bit_width{};
    std::uint32_t bit_width{};
    bool overridden{};
    std::uint64_t least_significant_bit{};
    std::optional<std::uint64_t> most_significant_bit;
    std::optional<std::uint64_t> maximum_unsigned_value;
};

struct PackedAggregateAnalysis {
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_storage_bytes;
    std::optional<std::uint64_t> total_payload_bits;
    std::optional<std::uint64_t> total_unused_bits;
    std::optional<std::uint64_t> cache_line_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> complete_elements_per_cache_line;
    std::optional<std::uint64_t> page_bytes;
    std::optional<std::uint64_t> minimum_pages;
    std::optional<std::uint64_t> complete_elements_per_page;
};

struct PackedAnalysis {
    lispb::schema::TypeId type;
    std::string schema_storage_type;
    std::string storage_type;
    bool storage_overridden{};
    std::optional<TypeFacts> storage_facts;
    std::optional<std::uint64_t> storage_bits;
    std::optional<std::uint64_t> bits_used;
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

struct RecordAnalysis {
    lispb::schema::TypeId type;
    std::vector<RecordMemberAnalysis> members;
    std::optional<std::uint64_t> payload_bytes;
    std::optional<std::uint64_t> internal_padding_bytes;
    std::optional<std::uint64_t> tail_padding_bytes;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint64_t> alignment_bytes;
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
    std::vector<Diagnostic> diagnostics;
};

class Analyzer {
  public:
    static auto analyze_enum(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId type,
                             AbiProfile const& abi) -> EnumDomainAnalysis;
    static auto analyze_packed(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               Variant const& variant,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1) -> PackedAnalysis;
    static auto analyze_record(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               AbiProfile const& abi) -> RecordAnalysis;
    static auto analyze_soa(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId type,
                            Variant const& variant,
                            AbiProfile const& abi,
                            std::uint64_t default_capacity) -> SoaAnalysis;
};

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> std::optional<std::string>;

} // namespace ioj::layout
