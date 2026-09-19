#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/model.hpp>
#include <ioj/layout/workspace.hpp>

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
    std::string logical_type;
    PackedFieldKind kind{PackedFieldKind::unsigned_integer};
    std::uint32_t schema_bit_width{};
    std::uint32_t bit_width{};
    bool overridden{};
    std::uint64_t least_significant_bit{};
    std::optional<std::uint64_t> most_significant_bit;
    std::optional<std::uint64_t> maximum_unsigned_value;
};

struct PackedAnalysis {
    SchemaId id;
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
    SchemaId id;
    std::uint64_t capacity{};
    bool capacity_overridden{};
    std::vector<SoaColumnAnalysis> columns;
    std::optional<std::uint64_t> bytes_per_logical_element;
    std::optional<std::uint64_t> total_payload_bytes;
    std::vector<Diagnostic> diagnostics;
};

class Analyzer {
  public:
    static auto analyze(PackedLayout const& layout, Variant const& variant, AbiProfile const& abi)
        -> PackedAnalysis;
    static auto analyze(SoaLayout const& layout,
                        Variant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity) -> SoaAnalysis;
};

} // namespace ioj::layout
