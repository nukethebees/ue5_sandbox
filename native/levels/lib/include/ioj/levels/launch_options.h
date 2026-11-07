#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/launch_mode.h>
#include <ioj/levels/presentation_mode.h>
#include <ioj/levels/results_navigation.h>

#include <optional>

namespace ioj::levels {

namespace launch {
inline constexpr double default_time_scale{1.0};
inline constexpr double maximum_time_scale{100.0};

using TimeScale = ConstrainedValue<double,
                                   ConstrainedValuePolicy{
                                       [](double const value) constexpr noexcept -> bool {
                                           return value > 0.0 && value <= maximum_time_scale;
                                       },
                                       "Requested time scale must be finite and in (0, 100]."}>;
}

struct LaunchOptions {
    LaunchMode launch_mode{LaunchMode::Running};
    launch::TimeScale requested_time_scale{launch::default_time_scale};
    PresentationMode presentation_mode{PresentationMode::Visual};
    std::optional<PositiveFinite<double>> simulated_duration_seconds{};
    ResultsNavigation results_navigation{ResultsNavigation::None};
    ControlContext control_context{ControlContext::Player};
};

}
