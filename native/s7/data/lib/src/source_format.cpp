#include <ioj/s7/source_format.h>

#include <cmath>
#include <format>

namespace ioj::s7 {
auto format_real(double const value) -> std::string {
    if (std::isnan(value)) {
        return std::string{literals::nan};
    }
    if (std::isinf(value)) {
        return std::string{std::signbit(value) ? literals::negative_infinity
                                               : literals::positive_infinity};
    }

    auto result{std::format("{}", value)};
    if (result.find_first_of(".eE") == std::string::npos) {
        result += ".0";
    }
    return result;
}
}
