#include <ioj/linear_travel.h>

#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ioj {
auto advance_linear_travel(LinearTravelState& state,
                           Vector3d const location,
                           double const speed,
                           double const elapsed_seconds,
                           double const pause_seconds) noexcept -> LinearTravelStep {
    assert(is_finite(state.start) && is_finite(state.end) && is_finite(location));
    assert(std::isfinite(speed) && speed >= 0.);
    assert(std::isfinite(elapsed_seconds) && elapsed_seconds >= 0.);
    assert(std::isfinite(pause_seconds) && pause_seconds >= 0.);

    if (elapsed_seconds == 0. || state.start == state.end) {
        return {location, false};
    }
    if (state.pause_remaining > 0.) {
        state.pause_remaining = std::max(0., state.pause_remaining - elapsed_seconds);
        return {location, false};
    }

    auto const target{state.towards_end ? state.end : state.start};
    auto const next{move_towards(location, target, speed * elapsed_seconds)};
    auto const arrived{next == target};
    if (arrived) {
        state.towards_end = !state.towards_end;
        state.pause_remaining = pause_seconds;
    }
    return {next, arrived};
}
}
