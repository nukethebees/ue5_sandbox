#pragma once

#include <ioj/require.h>

#include <cmath>
#include <optional>
#include <source_location>

namespace ioj {
class Duration {
  public:
    Duration() noexcept = default;

    [[nodiscard]] static auto valid(double const seconds) noexcept -> bool {
        return std::isfinite(seconds) && seconds >= 0.0;
    }

    // Require finite, nonnegative seconds.
    [[nodiscard]] static auto
        make(double const seconds,
             std::source_location const location = std::source_location::current()) noexcept
        -> Duration {
        require(valid(seconds), "Duration seconds must be finite and nonnegative", location);
        return Duration{seconds};
    }

    // Return nullopt for an invalid duration.
    [[nodiscard]] static auto try_make(double const seconds) noexcept -> std::optional<Duration> {
        if (!valid(seconds)) {
            return std::nullopt;
        }
        return Duration{seconds};
    }

    [[nodiscard]] auto get_seconds() const noexcept -> double { return seconds_; }
  private:
    explicit Duration(double const seconds) noexcept
        : seconds_{seconds} {}

    double seconds_{};
};
}
