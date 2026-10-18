#pragma once

#include <ioj/layout/analyzer/packed_aggregate_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct PackedAnalysis {
    lispb::schema::TypeId type{};
    std::string schema_storage_type{};
    std::string storage_type{};
    bool storage_overridden{};
    std::optional<lispb::codegen::PackedByteOrder> byte_order{};
    lispb::codegen::PackedBitOrder bit_order{
        lispb::codegen::PackedBitOrder::least_significant_first};
    std::optional<TypeFacts> storage_facts{};
    std::optional<std::uint64_t> storage_bits{};
    std::optional<std::uint64_t> bits_used{};
    std::optional<std::uint64_t> payload_bits{};
    std::optional<std::uint64_t> reserved_bits{};
    std::optional<std::uint64_t> unused_bits{};
    std::optional<std::uint64_t> overflow_bits{};
    std::optional<std::uint64_t> invalid_raw_value{};
    std::vector<PackedFieldAnalysis> fields{};
    std::vector<Diagnostic> diagnostics{};
    PackedAggregateAnalysis aggregate{};
};

}
