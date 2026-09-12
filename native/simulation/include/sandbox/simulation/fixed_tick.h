#pragma once

namespace ml::simulation {
[[nodiscard]] auto initialise_tick_loop(double tick_rate,
                                        double time_scale,
                                        double& tick_period,
                                        double& accumulator) noexcept -> bool;
void add_tick_loop_time(double dt, double time_scale, double& accumulator) noexcept;
[[nodiscard]] auto try_consume_tick(double tick_period, double& accumulator) noexcept -> bool;
}
