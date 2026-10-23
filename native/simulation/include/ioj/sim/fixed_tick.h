#pragma once

namespace ioj::sim {
[[nodiscard]] auto tick_loop_period(double tick_rate) noexcept -> double;
void add_tick_loop_time(double dt, double time_scale, double& accumulator) noexcept;
[[nodiscard]] auto try_consume_tick(double tick_period, double& accumulator) noexcept -> bool;
}
