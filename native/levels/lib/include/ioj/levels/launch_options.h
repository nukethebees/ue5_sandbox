#pragma once

#include <ioj/levels/control_context.h>
#include <ioj/levels/launch_mode.h>
#include <ioj/levels/presentation_mode.h>
#include <ioj/levels/results_navigation.h>

#include <cmath>
#include <expected>
#include <optional>
#include <string>

namespace ioj::levels {

namespace launch {
inline constexpr double default_time_scale{1.0};
inline constexpr double maximum_time_scale{100.0};
inline constexpr double placeholder_duration_seconds{24.0 * 60.0 * 60.0};

[[nodiscard]] inline auto is_valid_time_scale(double const value) noexcept -> bool {
    return std::isfinite(value) && value > 0.0 && value <= maximum_time_scale;
}
}

struct LaunchOptions {
    LaunchMode launch_mode{LaunchMode::Running};
    double requested_time_scale{launch::default_time_scale};
    PresentationMode presentation_mode{PresentationMode::Visual};
    std::optional<double> simulated_duration_seconds{};
    ResultsNavigation results_navigation{ResultsNavigation::None};
    ControlContext control_context{ControlContext::Player};
};

[[nodiscard]] inline auto validate_launch_options(LaunchOptions const& options)
    -> std::expected<void, std::string> {
    if (!launch::is_valid_time_scale(options.requested_time_scale)) {
        return std::unexpected{"Requested time scale must be finite and in (0, 100]."};
    }
    if (options.simulated_duration_seconds &&
        (!std::isfinite(*options.simulated_duration_seconds) ||
         *options.simulated_duration_seconds <= 0.0)) {
        return std::unexpected{"Simulated duration must be finite and positive."};
    }
    if (options.presentation_mode == PresentationMode::SimulationOnly &&
        !options.simulated_duration_seconds) {
        return std::unexpected{"Simulation-only launch requires a duration."};
    }
    return {};
}

}
