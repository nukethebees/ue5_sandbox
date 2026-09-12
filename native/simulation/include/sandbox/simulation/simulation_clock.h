#pragma once

#include <cstdint>

namespace ml::simulation {
[[nodiscard]] auto frequency_to_tick_period(double tick_rate, double frequency) noexcept
    -> std::uint64_t;
[[nodiscard]] auto duration_to_tick_period(double tick_rate, double duration) noexcept
    -> std::uint64_t;
[[nodiscard]] auto simulation_time(std::uint64_t completed_ticks, double tick_period) noexcept
    -> double;
}
