#include <sandbox/simulation_benchmark/benchmark_runner.hpp>

#include <ioj/sim/fighter_types.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/reference_level_simulation_data.h>
#include <ioj/sim/rotator3d.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/telemetry/level_telemetry_run_end_reason.h>
#include <sandbox/level_authoring/LevelDefinitionReader.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <vector>

namespace ml::simulation_benchmark {
namespace {
struct FrameMemorySummary {
    std::size_t peak_claimed_bytes{};
    std::size_t peak_payload_bytes{};
    std::uint64_t total_padding_bytes{};
    std::uint64_t total_root_claims{};
    std::int32_t peak_fighters{};

    void record_tick(ioj::sim::LevelSim const& simulation) {
        auto const stats{simulation.get_frame_memory_stats()};
        peak_claimed_bytes = std::max(peak_claimed_bytes, stats.last_frame_claimed_bytes);
        peak_payload_bytes = std::max(peak_payload_bytes, stats.last_frame_payload_bytes);
        total_padding_bytes += stats.last_frame_padding_bytes;
        total_root_claims += stats.last_frame_root_claim_count;
        peak_fighters = std::max(peak_fighters, simulation.get_fighters().get_num_instances());
    }
};

struct TickTimingSummary {
    double median_microseconds{};
    double p95_microseconds{};
    double p99_microseconds{};
};

auto summarize_tick_timings(std::vector<double> timings) -> TickTimingSummary {
    if (timings.empty()) {
        return {};
    }

    std::ranges::sort(timings);
    auto const count{timings.size()};
    auto const median{count % 2 == 0 ? (timings[count / 2 - 1] + timings[count / 2]) / 2.0
                                     : timings[count / 2]};
    auto const nearest_rank = [&timings, count](double const quantile) {
        auto const rank{static_cast<std::size_t>(std::ceil(quantile * static_cast<double>(count)))};
        return timings[std::clamp(rank, std::size_t{1}, count) - 1];
    };
    return {.median_microseconds = median,
            .p95_microseconds = nearest_rank(0.95),
            .p99_microseconds = nearest_rank(0.99)};
}

auto read_error(ml::level_authoring::LevelDefinitionReadResult const& result) -> std::string {
    std::ostringstream output;
    if (!result.script_error.empty()) {
        output << result.script_error;
    }
    for (auto const& error : result.decode_errors) {
        output << (output.tellp() > 0 ? "\n" : "") << error.path << ": " << error.message;
    }
    for (auto const& error : result.validation_errors) {
        output << (output.tellp() > 0 ? "\n" : "") << error.message;
    }
    return output.str();
}

auto mission_state_name(ioj::sim::MissionState const state) -> std::string {
    switch (state) {
        case ioj::sim::MissionState::NotStarted:
            return "not_started";
        case ioj::sim::MissionState::Running:
            return "running";
        case ioj::sim::MissionState::Succeeded:
            return "succeeded";
        case ioj::sim::MissionState::Failed:
            return "failed";
        case ioj::sim::MissionState::Disabled:
            return "disabled";
    }
    return "unknown";
}

auto json_string(std::string_view const value) -> std::string {
    std::ostringstream output;
    output << '"';
    for (auto const character : value) {
        switch (character) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned int>(static_cast<unsigned char>(character))
                           << std::dec;
                } else {
                    output << character;
                }
        }
    }
    output << '"';
    return output.str();
}
} // namespace

auto calculate_tick_count(double const seconds) -> std::expected<ioj::sim::SimTick, std::string> {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
        return std::unexpected{"seconds must be positive and finite"};
    }

    auto const ticks{std::ceil(seconds * simulation_tick_rate_hz)};
    auto const maximum{static_cast<double>(std::numeric_limits<ioj::sim::SimTick>::max())};
    if (!std::isfinite(ticks) || ticks >= maximum) {
        return std::unexpected{"seconds produces a tick count that is too large"};
    }
    return ioj::sim::duration_to_tick_period(simulation_tick_rate_hz, seconds);
}

auto run_benchmark(BenchmarkOptions const& options, ProfilerReadyCallback const profiler_ready)
    -> std::expected<BenchmarkResult, std::string> {
    auto const requested_ticks{calculate_tick_count(options.simulated_seconds)};
    if (!requested_ticks) {
        return std::unexpected{requested_ticks.error()};
    }

    ioj::sim::SimTick warmup_ticks{};
    ioj::sim::SimTick saturation_timeout_ticks{};
    if (options.fighter_stress_cap.has_value()) {
        if (*options.fighter_stress_cap <= 0) {
            return std::unexpected{"fighter stress cap must be positive"};
        }
        if (!std::isfinite(options.warmup_seconds) || options.warmup_seconds < 0.0) {
            return std::unexpected{"warm-up seconds must be finite and non-negative"};
        }
        if (options.warmup_seconds > 0.0) {
            auto const ticks{calculate_tick_count(options.warmup_seconds)};
            if (!ticks) {
                return std::unexpected{"invalid warm-up duration: " + ticks.error()};
            }
            warmup_ticks = *ticks;
        }
        auto const timeout_ticks{calculate_tick_count(options.saturation_timeout_seconds)};
        if (!timeout_ticks) {
            return std::unexpected{"invalid saturation timeout: " + timeout_ticks.error()};
        }
        saturation_timeout_ticks = *timeout_ticks;
    }

    if (options.profiler_connection_timeout_seconds.has_value()) {
        if (!ioj::sim::profiling::available) {
            return std::unexpected{"profiler support is not enabled in this benchmark build"};
        }

        if (profiler_ready != nullptr) {
            profiler_ready();
        }

        auto const timeout{
            std::chrono::duration<double>{*options.profiler_connection_timeout_seconds}};
        auto const deadline{std::chrono::steady_clock::now() + timeout};
        while (!ioj::sim::profiling::is_connected()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return std::unexpected{"timed out waiting for a profiler connection"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

#if defined(SANDBOX_WITH_TRACY)
    ZoneScopedN("Fighter simulation benchmark");
    if (options.fighter_stress_cap.has_value()) {
        ZoneNameF("Fighter simulation benchmark: %d fighters", *options.fighter_stress_cap);
    }
#endif

    ml::level_authoring::LevelDefinitionReader reader;
    auto level_result{reader.read_file(options.level_path)};
    if (!level_result) {
        return std::unexpected{read_error(level_result)};
    }
    auto const& level{*level_result.definition};

    auto reference{ioj::sim::make_reference_level_simulation_data()};
    auto& data{reference.data};
    data.clock_settings.tick_rate = simulation_tick_rate_hz;
    data.clock_settings.time_scale = static_cast<double>(options.game_speed);
    data.clock_settings.accumulator = 0.0;
    data.grid_dimensions.z = std::max(data.grid_dimensions.z, 25);
    if (options.fighter_stress_cap.has_value()) {
        data.fighters.max_live_fighters = *options.fighter_stress_cap;
        data.fighters.laser.damage = 0;
        data.fighters.health = std::numeric_limits<ioj::sim::Health>::max() / 2;
        data.capital_ships.max_health = std::numeric_limits<ioj::sim::Health>::max() / 2;
    }

    for (auto const& team_id : level.teams) {
        data.participating_teams.add(ioj::sim::levels::to_simulation_team(team_id));
    }
    if (options.fighter_stress_cap.has_value()) {
        auto const team_count{data.participating_teams.num()};
        if (team_count == 0 || *options.fighter_stress_cap % team_count != 0) {
            return std::unexpected{
                "fighter stress cap must divide evenly across participating teams"};
        }
    }

    if (level.player_entity_id.empty()) {
        data.player.reset();
    } else {
        auto const player_definition{std::ranges::find(
            level.entities, level.player_entity_id, &ioj::sim::levels::EntitySpawnDefinition::id)};
        auto player{reference.player};
        player.team = ioj::sim::levels::to_simulation_team(player_definition->team);
        player.transform.location = {player_definition->position.x,
                                     player_definition->position.y,
                                     player_definition->position.z};
        player.transform.rotation =
            ioj::sim::to_quaternion(ioj::sim::Rotator3d{player_definition->rotation.pitch,
                                                        player_definition->rotation.yaw,
                                                        player_definition->rotation.roll});
        data.player = player;
    }

    ioj::sim::SimClock compilation_clock;
    compilation_clock.initialise(data.clock_settings);
    auto compiled{ioj::sim::levels::compile_level(
        level, compilation_clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        std::ostringstream output;
        for (auto const& error : compiled.error()) {
            output << (output.tellp() > 0 ? "\n" : "") << error;
        }
        return std::unexpected{output.str()};
    }
    data.level_events = std::move(*compiled);

    if (options.telemetry_enabled) {
        data.telemetry_metadata = ioj::sim::LevelTelemetryRunMetadata{
            .level_id = level.metadata.id,
            .level_display_name = level.metadata.title,
            .tick_rate_hz = simulation_tick_rate_hz,
            .tick_period_seconds = 1.0 / simulation_tick_rate_hz,
            .initial_requested_time_scale = static_cast<double>(options.game_speed),
            .requested_duration_seconds = options.simulated_seconds,
        };
    }

    ioj::sim::LevelSim simulation{std::move(data)};

    simulation.finish_initialisation();
    auto const initial_capital_ships{simulation.get_capital_ships().get_num_instances()};
    auto const initial_turrets{simulation.get_turrets().get_num_instances()};
    simulation.set_time_scale(1.0);
    simulation.start();

    auto const tick_period{simulation.get_clock().get_tick_period()};
    std::uint64_t advance_calls{};
    auto const advance_one_tick = [&]() -> std::expected<void, std::string> {
        auto const previous_ticks{simulation.get_clock().get_completed_ticks()};
        simulation.advance(tick_period);
        ++advance_calls;
        if (simulation.get_clock().get_completed_ticks() == previous_ticks) {
            return std::unexpected{"simulation advance did not complete a deterministic tick"};
        }
        return {};
    };

    auto const total_started_at{std::chrono::steady_clock::now()};
    ioj::sim::SimTick saturation_ticks{};
    if (options.fighter_stress_cap.has_value()) {
        auto const cap{*options.fighter_stress_cap};
        while (simulation.get_fighters().get_num_instances() < cap &&
               saturation_ticks < saturation_timeout_ticks) {
            auto const advanced{advance_one_tick()};
            if (!advanced) {
                return std::unexpected{advanced.error()};
            }
            ++saturation_ticks;
        }
        auto const saturated_count{simulation.get_fighters().get_num_instances()};
        if (saturated_count != cap) {
            return std::unexpected{
                "fighter population did not reach configured cap " + std::to_string(cap) +
                " within " + std::to_string(options.saturation_timeout_seconds) +
                " simulated seconds; reached " + std::to_string(saturated_count)};
        }

        auto const spawned_at_saturation{simulation.get_capital_ships().get_fighters_spawned()};
        for (ioj::sim::SimTick tick{}; tick < warmup_ticks; ++tick) {
            auto const advanced{advance_one_tick()};
            if (!advanced) {
                return std::unexpected{advanced.error()};
            }
            if (simulation.get_fighters().get_num_instances() != cap) {
                return std::unexpected{"fighter population changed during post-saturation warm-up"};
            }
        }
        if (simulation.get_capital_ships().get_fighters_spawned() != spawned_at_saturation) {
            return std::unexpected{"replacement fighters spawned during post-saturation warm-up"};
        }
    }

    auto const steady_state_fighters{simulation.get_fighters().get_num_instances()};
    auto minimum_measured_fighters{steady_state_fighters};
    auto maximum_measured_fighters{steady_state_fighters};
    auto const fighters_spawned_before_measurement{
        simulation.get_capital_ships().get_fighters_spawned()};
    auto const lasers_spawned_before_measurement{simulation.get_lasers().get_number_spawned()};
    FrameMemorySummary frame_summary;
    std::vector<double> tick_microseconds;
    tick_microseconds.reserve(static_cast<std::size_t>(*requested_ticks));

    auto const started_at{std::chrono::steady_clock::now()};
    for (ioj::sim::SimTick tick{}; tick < *requested_ticks; ++tick) {
        auto const tick_started_at{std::chrono::steady_clock::now()};
        auto const advanced{advance_one_tick()};
        if (!advanced) {
            return std::unexpected{advanced.error()};
        }
        auto const tick_finished_at{std::chrono::steady_clock::now()};
        tick_microseconds.push_back(
            std::chrono::duration<double, std::micro>{tick_finished_at - tick_started_at}.count());
        frame_summary.record_tick(simulation);

        auto const fighter_count{simulation.get_fighters().get_num_instances()};
        minimum_measured_fighters = std::min(minimum_measured_fighters, fighter_count);
        maximum_measured_fighters = std::max(maximum_measured_fighters, fighter_count);
        if (options.fighter_stress_cap.has_value() &&
            fighter_count != *options.fighter_stress_cap) {
            return std::unexpected{"fighter population changed during measured ticks"};
        }
    }
    auto const finished_at{std::chrono::steady_clock::now()};
    auto const fighter_spawns_during_measurement{
        simulation.get_capital_ships().get_fighters_spawned() -
        fighters_spawned_before_measurement};
    if (options.fighter_stress_cap.has_value() && fighter_spawns_during_measurement != 0) {
        return std::unexpected{"replacement fighters spawned during measured ticks"};
    }

    if (options.telemetry_enabled) {
        simulation.complete_telemetry_run(ioj::sim::LevelTelemetryRunEndReason::DurationReached);
    } else {
        simulation.pause();
    }

    auto const frame_memory{simulation.get_frame_memory_stats()};
    auto const telemetry_history{simulation.get_level_telemetry_manager().get_history_stats()};
    [[maybe_unused]] auto telemetry_run{simulation.take_finalized_telemetry_run()};
    auto const total_simulation_ticks{simulation.get_clock().get_completed_ticks()};
    auto const elapsed{std::chrono::duration<double>{finished_at - started_at}.count()};
    auto const total_elapsed{std::chrono::duration<double>{finished_at - total_started_at}.count()};
    auto const timing_summary{summarize_tick_timings(std::move(tick_microseconds))};
    auto const fighter_tasks{simulation.get_fighters().get_tasks()};
    auto const count_task = [fighter_tasks](ioj::sim::FighterTask const task) {
        return static_cast<std::int32_t>(std::ranges::count(fighter_tasks, task));
    };
    return BenchmarkResult{
        .level_path = options.level_path.generic_string(),
        .level_id = level.metadata.id,
        .level_title = level.metadata.title,
        .requested_seconds = options.simulated_seconds,
        .tick_rate_hz = simulation.get_clock().get_tick_rate(),
        .game_speed = options.game_speed,
        .requested_ticks = *requested_ticks,
        .completed_ticks = *requested_ticks,
        .advance_calls = advance_calls,
        .completed_seconds = static_cast<double>(*requested_ticks) / simulation_tick_rate_hz,
        .elapsed_seconds = elapsed,
        .total_elapsed_seconds = total_elapsed,
        .saturation_ticks = saturation_ticks,
        .warmup_ticks = warmup_ticks,
        .measured_ticks = *requested_ticks,
        .total_simulation_ticks = total_simulation_ticks,
        .median_tick_microseconds = timing_summary.median_microseconds,
        .p95_tick_microseconds = timing_summary.p95_microseconds,
        .p99_tick_microseconds = timing_summary.p99_microseconds,
        .fighter_stress_enabled = options.fighter_stress_cap.has_value(),
        .configured_fighter_cap = options.fighter_stress_cap.value_or(0),
        .steady_state_fighters = steady_state_fighters,
        .minimum_measured_fighters = minimum_measured_fighters,
        .maximum_measured_fighters = maximum_measured_fighters,
        .fighter_spawns_during_measurement = fighter_spawns_during_measurement,
        .lasers_spawned_during_measurement =
            simulation.get_lasers().get_number_spawned() - lasers_spawned_before_measurement,
        .standby_fighters = count_task(ioj::sim::FighterTask::Standby),
        .moving_fighters = count_task(ioj::sim::FighterTask::MoveToDestination),
        .attacking_fighters = count_task(ioj::sim::FighterTask::Attack),
        .initial_capital_ships = initial_capital_ships,
        .initial_turrets = initial_turrets,
        .alive_entities = simulation.get_entity_ledger().count_alive(),
        .capital_ships = simulation.get_capital_ships().get_num_instances(),
        .fighters = simulation.get_fighters().get_num_instances(),
        .turrets = simulation.get_turrets().get_num_instances(),
        .spinners = simulation.get_spinners().get_num_instances(),
        .active_lasers = simulation.get_lasers().get_num_instances(),
        .lasers_spawned = simulation.get_lasers().get_number_spawned(),
        .peak_fighters = frame_summary.peak_fighters,
        .mission_state = mission_state_name(simulation.get_mission_manager().get_mission_state()),
        .frame_memory_capacity_bytes = frame_memory.capacity_bytes,
        .frame_memory_peak_claimed_bytes = frame_summary.peak_claimed_bytes,
        .frame_memory_peak_payload_bytes = frame_summary.peak_payload_bytes,
        .frame_memory_total_padding_bytes = frame_summary.total_padding_bytes,
        .frame_memory_total_root_claims = frame_summary.total_root_claims,
        .frame_memory_overflow_count = frame_memory.overflow_count,
        .telemetry_enabled = options.telemetry_enabled,
        .telemetry_rows = telemetry_history.used_sample_count,
        .telemetry_acquired_blocks = telemetry_history.acquired_block_count,
        .telemetry_retained_blocks = telemetry_history.retained_block_count,
        .telemetry_allocated_bytes = telemetry_history.total_byte_capacity,
        .hardware_threads = std::thread::hardware_concurrency(),
        .compiler = SANDBOX_BENCHMARK_COMPILER_ID,
        .build_type = SANDBOX_BENCHMARK_BUILD_TYPE,
#ifdef SANDBOX_BENCHMARK_WITH_TRACY
        .tracy_enabled = true,
#else
        .tracy_enabled = false,
#endif
    };
}

auto to_json(BenchmarkResult const& result) -> std::string {
    auto const ticks_per_second{result.elapsed_seconds > 0.0
                                    ? static_cast<double>(result.completed_ticks) /
                                          result.elapsed_seconds
                                    : 0.0};
    auto const mean_tick_microseconds{result.completed_ticks > 0
                                          ? result.elapsed_seconds * 1'000'000.0 /
                                                static_cast<double>(result.completed_ticks)
                                          : 0.0};
    auto const realtime_factor{result.tick_rate_hz > 0.0 ? ticks_per_second / result.tick_rate_hz
                                                         : 0.0};

    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << "{\"schema_version\":2"
           << ",\"level\":{\"path\":" << json_string(result.level_path)
           << ",\"id\":" << json_string(result.level_id)
           << ",\"title\":" << json_string(result.level_title) << "}"
           << ",\"workload\":{\"requested_seconds\":" << result.requested_seconds
           << ",\"tick_rate_hz\":" << result.tick_rate_hz << ",\"game_speed\":" << result.game_speed
           << ",\"requested_ticks\":" << result.requested_ticks
           << ",\"completed_ticks\":" << result.completed_ticks
           << ",\"completed_seconds\":" << result.completed_seconds
           << ",\"advance_calls\":" << result.advance_calls
           << ",\"saturation_ticks\":" << result.saturation_ticks
           << ",\"warmup_ticks\":" << result.warmup_ticks
           << ",\"measured_ticks\":" << result.measured_ticks
           << ",\"total_simulation_ticks\":" << result.total_simulation_ticks << "}"
           << ",\"timing\":{\"elapsed_seconds\":" << result.elapsed_seconds
           << ",\"total_elapsed_seconds\":" << result.total_elapsed_seconds
           << ",\"ticks_per_second\":" << ticks_per_second
           << ",\"realtime_factor\":" << realtime_factor
           << ",\"mean_tick_microseconds\":" << mean_tick_microseconds
           << ",\"median_tick_microseconds\":" << result.median_tick_microseconds
           << ",\"p95_tick_microseconds\":" << result.p95_tick_microseconds
           << ",\"p99_tick_microseconds\":" << result.p99_tick_microseconds << "}"
           << ",\"fighter_stress\":{\"enabled\":"
           << (result.fighter_stress_enabled ? "true" : "false")
           << ",\"configured_cap\":" << result.configured_fighter_cap
           << ",\"steady_state_fighters\":" << result.steady_state_fighters
           << ",\"minimum_measured_fighters\":" << result.minimum_measured_fighters
           << ",\"maximum_measured_fighters\":" << result.maximum_measured_fighters
           << ",\"fighter_spawns_during_measurement\":" << result.fighter_spawns_during_measurement
           << ",\"lasers_spawned_during_measurement\":" << result.lasers_spawned_during_measurement
           << ",\"task_counts\":{\"standby\":" << result.standby_fighters
           << ",\"move_to_destination\":" << result.moving_fighters
           << ",\"attack\":" << result.attacking_fighters << "}}"
           << ",\"final_state\":{\"mission_state\":" << json_string(result.mission_state)
           << ",\"initial_capital_ships\":" << result.initial_capital_ships
           << ",\"initial_turrets\":" << result.initial_turrets
           << ",\"alive_entities\":" << result.alive_entities
           << ",\"capital_ships\":" << result.capital_ships << ",\"fighters\":" << result.fighters
           << ",\"turrets\":" << result.turrets << ",\"spinners\":" << result.spinners
           << ",\"active_lasers\":" << result.active_lasers
           << ",\"lasers_spawned\":" << result.lasers_spawned
           << ",\"peak_fighters\":" << result.peak_fighters << "}"
           << ",\"memory\":{\"frame_capacity_bytes\":" << result.frame_memory_capacity_bytes
           << ",\"frame_peak_claimed_bytes\":" << result.frame_memory_peak_claimed_bytes
           << ",\"frame_peak_payload_bytes\":" << result.frame_memory_peak_payload_bytes
           << ",\"frame_total_padding_bytes\":" << result.frame_memory_total_padding_bytes
           << ",\"frame_total_root_claims\":" << result.frame_memory_total_root_claims
           << ",\"frame_overflow_count\":" << result.frame_memory_overflow_count << "}"
           << ",\"telemetry\":{\"enabled\":" << (result.telemetry_enabled ? "true" : "false")
           << ",\"rows\":" << result.telemetry_rows
           << ",\"acquired_blocks\":" << result.telemetry_acquired_blocks
           << ",\"retained_blocks\":" << result.telemetry_retained_blocks
           << ",\"allocated_bytes\":" << result.telemetry_allocated_bytes << "}"
           << ",\"environment\":{\"hardware_threads\":" << result.hardware_threads
           << ",\"compiler\":" << json_string(result.compiler)
           << ",\"build_type\":" << json_string(result.build_type)
           << ",\"tracy_enabled\":" << (result.tracy_enabled ? "true" : "false") << "}}";
    return output.str();
}
} // namespace ml::simulation_benchmark
