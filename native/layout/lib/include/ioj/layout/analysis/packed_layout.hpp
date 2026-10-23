#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
namespace ioj::layout {
struct PackedLayout {
    RowRange fields{};
    lispb::IdentifierId schema_storage_type{};
    lispb::IdentifierId storage_type{};
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
};
}
