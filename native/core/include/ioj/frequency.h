#pragma once

#include <ioj/duration.h>

#include <cassert>
#include <cmath>

namespace ioj {
struct Frequency {
    explicit Frequency(double const hz) noexcept { set_hz(hz); }

    auto get_hz() const noexcept -> double { return hz_; }
    auto period() const noexcept -> Duration { return Duration{1.0 / hz_}; }
    void set_hz(double const hz) noexcept {
        auto const valid{is_valid_hz(hz)};
        assert(valid);
        hz_ = valid ? hz : 1.0;
    }

    static auto is_valid_hz(double const hz) noexcept -> bool {
        return std::isfinite(hz) && hz > 0.0 && std::isfinite(1.0 / hz);
    }
  private:
    double hz_{};
};
}
