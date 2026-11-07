#pragma once

#include <ioj/constrained_numbers.h>

#include <expected>
#include <string>
#include <string_view>

namespace lispb::codegen {

[[nodiscard]] auto parse_fixed_point_endpoint(std::string_view text)
    -> std::expected<ioj::Finite<double>, std::string>;

} // namespace lispb::codegen
