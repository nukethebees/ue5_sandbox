#pragma once

#include <cassert>
#include <cmath>

namespace ioj {
struct Duration {
    explicit Duration(double const seconds = 0.0) noexcept { set_seconds(seconds); }

    auto get_seconds() const noexcept -> double { return seconds_; }
    void set_seconds(double const seconds) noexcept {
        auto const valid{std::isfinite(seconds) && seconds >= 0.0};
        assert(valid);
        seconds_ = valid ? seconds : 1.0;
    }
  private:
    double seconds_{};
};
}
