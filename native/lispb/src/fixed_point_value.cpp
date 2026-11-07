#include <codegen/schema/fixed_point_value.h>

#include <charconv>
#include <cmath>
#include <system_error>

namespace lispb::codegen {

auto parse_fixed_point_endpoint(std::string_view text) -> std::expected<double, std::string> {
    if (text.starts_with('+')) {
        text.remove_prefix(1);
    }
    double value{};
    auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) {
        return std::unexpected{"Expected a finite fixed-point range endpoint."};
    }
    return value;
}

} // namespace lispb::codegen
