#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace lispb::codegen {

[[nodiscard]] auto parse_fixed_point_endpoint(std::string_view text)
    -> std::expected<double, std::string>;

} // namespace lispb::codegen
