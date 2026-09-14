#pragma once

#include <ioj/sim/sim_tick.h>
#include <sandbox/simulation_benchmark/command_line.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace ml::simulation_benchmark {
inline constexpr double simulation_tick_rate_hz{60.0};

struct BenchmarkResult {
    std::string level_path{};
    std::string level_id{};
    std::string level_title{};
    double requested_seconds{};
    double tick_rate_hz{};
    ioj::sim::SimTick requested_ticks{};
    ioj::sim::SimTick completed_ticks{};
    double completed_seconds{};
    double elapsed_seconds{};
    std::int32_t alive_entities{};
    std::int32_t capital_ships{};
    std::int32_t fighters{};
    std::int32_t turrets{};
    std::int32_t spinners{};
    std::int32_t active_lasers{};
    std::int32_t lasers_spawned{};
    std::string mission_state{};
    std::size_t frame_memory_capacity_bytes{};
    std::size_t frame_memory_peak_claimed_bytes{};
    std::uint64_t frame_memory_overflow_count{};
    unsigned int hardware_threads{};
    std::string compiler{};
    std::string build_type{};
    bool tracy_enabled{};
};

[[nodiscard]] auto calculate_tick_count(double seconds)
    -> std::expected<ioj::sim::SimTick, std::string>;
[[nodiscard]] auto run_benchmark(BenchmarkOptions const& options)
    -> std::expected<BenchmarkResult, std::string>;
[[nodiscard]] auto to_json(BenchmarkResult const& result) -> std::string;
} // namespace ml::simulation_benchmark
