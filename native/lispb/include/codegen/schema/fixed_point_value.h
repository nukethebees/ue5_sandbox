#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace codegen {

[[nodiscard]] auto parse_fixed_point_value(std::string_view text, std::uint32_t fractional_bits)
    -> std::expected<PackedIntegerValue, std::string>;

} // namespace codegen
