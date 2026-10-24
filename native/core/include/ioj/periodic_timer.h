#pragma once

#include <ioj/constrained_numbers.h>

#include <cassert>
#include <cmath>

namespace ioj {
class PeriodicTimer {
  public:
    explicit PeriodicTimer(PositiveFinite<double> const period) noexcept
        : period_{period.get()} {}

    void reset() noexcept { next_time_ = 0; }

    // Supply monotonic elapsed seconds. Consume once immediately, then coalesce missed periods.
    [[nodiscard]] auto try_consume(double const elapsed_seconds) noexcept -> bool {
        assert(std::isfinite(elapsed_seconds) && elapsed_seconds >= 0);
        if (elapsed_seconds < next_time_) {
            return false;
        }

        next_time_ = (std::floor(elapsed_seconds / period_) + 1) * period_;
        return true;
    }
  private:
    double period_;
    double next_time_{};
};
}
