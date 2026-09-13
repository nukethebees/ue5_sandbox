#pragma once

namespace ml {
struct SpeedResponse {
    float settling_time{3.f};
    float damping_ratio{0.5f};

    auto tau() const noexcept -> float { return settling_time / 5.f; }
    auto natural_angular_frequency() const noexcept -> float { return 1.f / tau(); }
};

struct SpeedResponses {
    SpeedResponse boost{};
    SpeedResponse brake{};
    SpeedResponse slowing_to_cruise{};
    SpeedResponse accelerating_to_cruise{};
};
}
