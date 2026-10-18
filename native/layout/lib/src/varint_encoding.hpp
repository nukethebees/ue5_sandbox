#pragma once

#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/packed_integer_value.h>

#include <cstdint>

namespace ioj::layout::analysis_detail {

auto unsigned_varint_bytes(std::uint64_t value) -> std::uint32_t;

auto signed_integer_value(lispb::codegen::PackedIntegerValue const value) -> std::int64_t;

auto signed_varint_bytes(std::int64_t value) -> std::uint32_t;

auto zigzag_value(lispb::codegen::PackedIntegerValue const value) -> std::uint64_t;

auto integer_varint_bytes(lispb::codegen::PackedIntegerValue const value,
                          lispb::codegen::IntegerVarintEncoding const encoding) -> std::uint32_t;

}
