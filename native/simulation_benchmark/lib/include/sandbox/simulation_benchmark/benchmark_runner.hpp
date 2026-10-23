#pragma once

#include <ioj/frequency.h>
#include <ioj/sim/sim_tick.h>

#include <sandbox/simulation_benchmark/command_line.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace ioj::simulation_benchmark {
inline constexpr char profiler_ready_message[]{"native-simulation-benchmark: profiler-ready"};

using ProfilerReadyCallback = void (*)();

struct BenchmarkResult {
    std::string level_path{};
    std::string level_id{};
    std::string level_title{};
    double requested_seconds{};
    double tick_rate_hz{};
    std::uint32_t game_speed{};
    sim::SimTick requested_ticks{};
    sim::SimTick completed_ticks{};
    std::uint64_t advance_calls{};
    double completed_seconds{};
    double elapsed_seconds{};
    double total_elapsed_seconds{};
    sim::SimTick saturation_ticks{};
    sim::SimTick warmup_ticks{};
    sim::SimTick measured_ticks{};
    sim::SimTick total_simulation_ticks{};
    double median_tick_microseconds{};
    double p95_tick_microseconds{};
    double p99_tick_microseconds{};
    bool fighter_stress_enabled{};
    std::int32_t configured_fighter_cap{};
    std::uint32_t steady_state_fighters{};
    std::uint32_t minimum_measured_fighters{};
    std::uint32_t maximum_measured_fighters{};
    std::uint32_t fighter_spawns_during_measurement{};
    std::uint32_t lasers_spawned_during_measurement{};
    std::uint32_t standby_fighters{};
    std::uint32_t moving_fighters{};
    std::uint32_t attacking_fighters{};
    std::uint32_t initial_capital_ships{};
    std::uint32_t initial_turrets{};
    std::int32_t alive_entities{};
    std::uint32_t capital_ships{};
    std::uint32_t fighters{};
    std::uint32_t turrets{};
    std::uint32_t spinners{};
    std::uint32_t active_lasers{};
    std::uint32_t lasers_spawned{};
    std::uint32_t peak_fighters{};
    std::string mission_state{};
    std::size_t frame_memory_capacity_bytes{};
    std::size_t frame_memory_peak_claimed_bytes{};
    std::size_t frame_memory_peak_payload_bytes{};
    std::uint64_t frame_memory_total_padding_bytes{};
    std::uint64_t frame_memory_total_root_claims{};
    std::uint64_t frame_memory_overflow_count{};
    bool telemetry_enabled{};
    std::uint32_t telemetry_rows{};
    std::uint32_t telemetry_acquired_blocks{};
    std::uint32_t telemetry_retained_blocks{};
    std::size_t telemetry_allocated_bytes{};
    unsigned int hardware_threads{};
    std::string compiler{};
    std::string build_type{};
    bool tracy_enabled{};
};

[[nodiscard]] auto calculate_tick_count(double seconds,
                                        Frequency tick_rate_hz = Frequency::make(60.0))
    -> std::expected<sim::SimTick, std::string>;
[[nodiscard]] auto run_benchmark(BenchmarkOptions const& options,
                                 ProfilerReadyCallback profiler_ready = nullptr)
    -> std::expected<BenchmarkResult, std::string>;
[[nodiscard]] auto to_json(BenchmarkResult const& result) -> std::string;
} // namespace ioj::simulation_benchmark
