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
    std::optional<CacheLineTiling> cache_line_tiling;
};

struct SoaAnalysis {
    lispb::schema::TypeId type;
    std::uint64_t capacity{};
    bool capacity_overridden{};
    std::vector<SoaColumnAnalysis> columns;
    std::optional<std::uint64_t> bytes_per_logical_element;
    std::optional<std::uint64_t> total_payload_bytes;
    std::vector<Diagnostic> diagnostics;
};

class Analyzer {
  public:
    static auto analyze_packed(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               Variant const& variant,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1) -> PackedAnalysis;
    static auto analyze_soa(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId type,
                            Variant const& variant,
                            AbiProfile const& abi,
                            std::uint64_t default_capacity) -> SoaAnalysis;
};

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> std::optional<std::string>;

} // namespace ioj::layout
