#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace ml::details {
template <std::size_t DecimalPlaces>
consteval auto make_decimal_scale() noexcept -> std::uintmax_t {
    std::uintmax_t scale{1};
    for (std::size_t place{}; place < DecimalPlaces; ++place) {
        scale *= 10;
    }
    return scale;
}
}

namespace ml {
template <std::signed_integral IntegerType,
          std::unsigned_integral FractionType,
          std::size_t DecimalPlaces>
    requires (!std::same_as<FractionType, bool> &&
              DecimalPlaces <= std::numeric_limits<std::uintmax_t>::digits10 &&
              details::make_decimal_scale<DecimalPlaces>() - 1 <=
                  std::numeric_limits<FractionType>::max())
class TQuantizedDecimal {
  public:
    using integer_type = IntegerType;
    using fraction_type = FractionType;

    static constexpr std::size_t decimal_places{DecimalPlaces};
    static constexpr std::uintmax_t decimal_scale{details::make_decimal_scale<DecimalPlaces>()};
    static constexpr fraction_type max_fraction{static_cast<fraction_type>(decimal_scale - 1)};

    TQuantizedDecimal() noexcept = default;

    template <std::floating_point FloatingPoint>
    explicit TQuantizedDecimal(FloatingPoint const value) noexcept {
        quantize(value);
    }

    auto operator==(TQuantizedDecimal const&) const noexcept -> bool = default;

    [[nodiscard]] constexpr auto integer() const noexcept -> integer_type { return integer_; }

    [[nodiscard]] constexpr auto fraction() const noexcept -> fraction_type { return fraction_; }

    template <std::floating_point FloatingPoint = double>
    [[nodiscard]] constexpr auto to_floating_point() const noexcept -> FloatingPoint {
        return static_cast<FloatingPoint>(integer_) +
               static_cast<FloatingPoint>(fraction_) / static_cast<FloatingPoint>(decimal_scale);
    }
  private:
    template <std::floating_point FloatingPoint>
    void quantize(FloatingPoint const input) noexcept {
        auto const value{static_cast<long double>(input)};
        if (std::isnan(value)) {
            return;
        }

        constexpr auto minimum_integer{std::numeric_limits<integer_type>::min()};
        constexpr auto maximum_integer{std::numeric_limits<integer_type>::max()};
        constexpr auto minimum_value{static_cast<long double>(minimum_integer)};
        constexpr auto maximum_value{static_cast<long double>(maximum_integer) +
                                     static_cast<long double>(max_fraction) /
                                         static_cast<long double>(decimal_scale)};

        if (value <= minimum_value) {
            integer_ = minimum_integer;
            fraction_ = 0;
            return;
        }

        if (value >= maximum_value) {
            integer_ = maximum_integer;
            fraction_ = max_fraction;
            return;
        }

        auto const negative{std::signbit(value)};
        auto const magnitude{std::abs(value)};
        auto const whole_magnitude_value{std::floor(magnitude)};
        auto whole_magnitude{static_cast<std::uintmax_t>(whole_magnitude_value)};
        auto fraction_magnitude{static_cast<std::uintmax_t>(std::floor(
            (magnitude - whole_magnitude_value) * static_cast<long double>(decimal_scale) + 0.5L))};

        if (fraction_magnitude == decimal_scale) {
            ++whole_magnitude;
            fraction_magnitude = 0;
        }

        if (!negative || (whole_magnitude == 0 && fraction_magnitude == 0)) {
            integer_ = static_cast<integer_type>(whole_magnitude);
            fraction_ = static_cast<fraction_type>(fraction_magnitude);
            return;
        }

        if (fraction_magnitude == 0) {
            integer_ = whole_magnitude == minimum_magnitude()
                         ? minimum_integer
                         : static_cast<integer_type>(-static_cast<std::intmax_t>(whole_magnitude));
            fraction_ = 0;
            return;
        }

        auto const floor_magnitude{whole_magnitude + 1};
        integer_ = floor_magnitude == minimum_magnitude()
                     ? minimum_integer
                     : static_cast<integer_type>(-static_cast<std::intmax_t>(floor_magnitude));
        fraction_ = static_cast<fraction_type>(decimal_scale - fraction_magnitude);
    }

    [[nodiscard]] static constexpr auto minimum_magnitude() noexcept -> std::uintmax_t {
        constexpr auto minimum_integer{std::numeric_limits<integer_type>::min()};
        return static_cast<std::uintmax_t>(-(minimum_integer + 1)) + 1;
    }

    // The integer is the floor of the represented value. The non-negative fraction is the
    // decimal offset above it, so -0.42 is stored as {-1, 58} at two decimal places.
    integer_type integer_{0};
    fraction_type fraction_{0};
};
}
