#pragma once

#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace ioj::levels {
enum class ControlContext : std::uint8_t { None, Player, Observer, Benchmark };
enum class LaunchMode : std::uint8_t { Running, Paused };
enum class PresentationMode : std::uint8_t { Visual, SimulationOnly };
enum class ResultsNavigation : std::uint8_t { None, Telemetry };

namespace launch {
inline constexpr double default_time_scale{1.0};
inline constexpr double maximum_time_scale{100.0};

[[nodiscard]] inline auto is_valid_time_scale(double const value) noexcept -> bool {
    return std::isfinite(value) && value > 0.0 && value <= maximum_time_scale;
}
}

struct LaunchOptions {
    LaunchMode launch_mode{LaunchMode::Running};
    double requested_time_scale{launch::default_time_scale};
    PresentationMode presentation_mode{PresentationMode::Visual};
    std::optional<double> simulated_duration_seconds{};
    bool stop_when_battle_resolved{};
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
        !options.simulated_duration_seconds && !options.stop_when_battle_resolved) {
        return std::unexpected{
            "Simulation-only launch requires a duration or battle-resolution limit."};
    }
    return {};
}

class ModalResumeState {
  public:
    void suspend(ControlContext const context) {
        context_ = context;
        pending_ = true;
    }
    void clear() {
        context_ = ControlContext::None;
        pending_ = false;
    }
    void on_player_changed(bool const has_player) {
        if (pending_) {
            if (has_player) {
                context_ = ControlContext::Player;
            } else if (context_ == ControlContext::Player) {
                context_ = ControlContext::None;
            }
        }
    }
    [[nodiscard]] auto context(ControlContext const fallback) const -> ControlContext {
        return pending_ ? context_ : fallback;
    }
    [[nodiscard]] auto pending() const -> bool { return pending_; }
  private:
    ControlContext context_{ControlContext::None};
    bool pending_{};
};
}
