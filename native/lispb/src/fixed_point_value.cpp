#include <codegen/schema/fixed_point_value.h>

#include <charconv>
#include <system_error>

namespace lispb::codegen {

auto parse_fixed_point_endpoint(std::string_view text)
    -> std::expected<ioj::Finite<double>, std::string> {
    if (text.starts_with('+')) {
        text.remove_prefix(1);
    }
    double value{};
    auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
    auto const finite{ioj::Finite<double>::try_make(value)};
    if (error != std::errc{} || end != text.data() + text.size() || !finite) {
        return std::unexpected{"Expected a finite fixed-point range endpoint."};
    }
    return *finite;
}

} // namespace lispb::codegen
