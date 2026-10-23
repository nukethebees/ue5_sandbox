#pragma once

#include <ioj/duration.h>
#include <ioj/require.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <source_location>

namespace ioj {
class Frequency {
  public:
    static constexpr double min_hz{1e-300};

    explicit consteval Frequency(double const hz) noexcept
        : hz_{hz} {
        if (!valid(hz)) {
            std::abort();
        }
    }

    [[nodiscard]] static constexpr auto valid(double const hz) noexcept -> bool {
        return hz >= min_hz && hz <= std::numeric_limits<double>::max();
    }

    // Require a finite frequency of at least min_hz.
    [[nodiscard]] static auto
        make(double const hz,
             std::source_location const location = std::source_location::current()) noexcept
        -> Frequency {
        require(valid(hz), "Frequency must be finite and at least 1e-300 Hz", location);
        return Frequency{hz, Validated{}};
    }

    // Return nullopt for an invalid frequency.
    [[nodiscard]] static constexpr auto try_make(double const hz) noexcept
        -> std::optional<Frequency> {
        if (!valid(hz)) {
            return std::nullopt;
        }
        return Frequency{hz, Validated{}};
    }

    [[nodiscard]] constexpr auto get_hz() const noexcept -> double { return hz_; }
    [[nodiscard]] auto period() const noexcept -> Duration { return Duration::make(1.0 / hz_); }
  private:
    struct Validated {};

    explicit constexpr Frequency(double const hz, Validated) noexcept
        : hz_{hz} {}

    double hz_;
};
}
