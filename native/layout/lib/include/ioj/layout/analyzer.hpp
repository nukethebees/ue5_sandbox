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

struct PackedFieldAnalysis {
    std::string name;
    PackedFieldKind kind{PackedFieldKind::unsigned_integer};
    std::uint32_t bit_width{};
    std::uint64_t least_significant_bit{};
    std::optional<std::uint64_t> most_significant_bit;
    std::optional<std::uint64_t> maximum_unsigned_value;
};

struct PackedAnalysis {
    SchemaId id;
    std::string storage_type;
    std::optional<TypeFacts> storage_facts;
    std::optional<std::uint64_t> storage_bits;
    std::optional<std::uint64_t> bits_used;
    std::optional<std::uint64_t> unused_bits;
    std::optional<std::uint64_t> invalid_raw_value;
    std::vector<PackedFieldAnalysis> fields;
    std::vector<Diagnostic> diagnostics;
};

struct SoaColumnAnalysis {
    std::string name;
    std::string physical_type;
    std::optional<TypeFacts> type_facts;
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> minimum_cache_lines;
    std::optional<std::uint64_t> elements_per_cache_line;
};

struct SoaAnalysis {
    SchemaId id;
    std::uint64_t capacity{};
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
