#include <sandbox/simulation_benchmark/benchmark_runner.hpp>

#include <ioj/format_writer.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/rotator3d.h>
#include <ioj/s7/simulation_settings_reader.h>
#include <ioj/sim/fighters/fighter_task_format.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/reference_level_simulation_data.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <thread>
#include <vector>

namespace ioj::simulation_benchmark {
namespace {
struct FrameMemorySummary {
    void record_tick(sim::LevelSim const& simulation) {
        auto const stats{simulation.get_frame_memory_stats()};
        peak_claimed_bytes = std::max(peak_claimed_bytes, stats.last_frame_claimed_bytes);
        peak_payload_bytes = std::max(peak_payload_bytes, stats.last_frame_payload_bytes);
        total_padding_bytes += stats.last_frame_padding_bytes;
        total_root_claims += stats.last_frame_root_claim_count;
        peak_fighters = std::max(peak_fighters, simulation.get_fighters().get_num_instances());
    }

    std::size_t peak_claimed_bytes{};
    std::size_t peak_payload_bytes{};
    std::uint64_t total_padding_bytes{};
    std::uint64_t total_root_claims{};
    std::uint32_t peak_fighters{};
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

auto read_error(levels::authoring::LevelDefinitionReadResult const& result) -> std::string {
    return levels::format_diagnostics(result.error());
}

} // namespace

auto calculate_tick_count(double const seconds, Frequency const tick_rate_hz)
    -> std::expected<sim::SimTick, std::string> {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
        return std::unexpected{"seconds must be positive and finite"};
    }

    auto const ticks{std::ceil(seconds * tick_rate_hz.get_hz())};
    auto const maximum{static_cast<double>(std::numeric_limits<sim::SimTick>::max())};
    if (!std::isfinite(ticks) || ticks >= maximum) {
        return std::unexpected{"seconds produces a tick count that is too large"};
    }
    return sim::duration_to_tick_period(tick_rate_hz, Duration::make(seconds)).get();
}

auto run_benchmark(BenchmarkOptions const& options, ProfilerReadyCallback const profiler_ready)
    -> std::expected<BenchmarkResult, std::string> {
    auto const script_root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"};
    levels::authoring::DefinitionReader reader{script_root};
    auto level_result{reader.read_level_file(options.level_path)};
    if (!level_result) {
        return std::unexpected{read_error(level_result)};
    }
    auto const& level{*level_result};
    auto const& level_components{level};

    auto const loaded_settings{
        s7::read_simulation_settings(script_root / "Settings" / "base.scm", script_root, &level)};
    if (!loaded_settings) {
        return std::unexpected{levels::format_diagnostics(loaded_settings.error())};
    }
    auto const& settings{*loaded_settings};
    auto const tick_rate_hz{settings.clock.tick_rate};
    auto const requested_ticks{calculate_tick_count(options.simulated_seconds, tick_rate_hz)};
    if (!requested_ticks) {
        return std::unexpected{requested_ticks.error()};
    }

    sim::SimTick warmup_ticks{};
    sim::SimTick saturation_timeout_ticks{};
    if (options.fighter_stress_cap.has_value()) {
        if (*options.fighter_stress_cap <= 0) {
            return std::unexpected{"fighter stress cap must be positive"};
        }
        if (!std::isfinite(options.warmup_seconds) || options.warmup_seconds < 0.0) {
            return std::unexpected{"warm-up seconds must be finite and non-negative"};
        }
        if (options.warmup_seconds > 0.0) {
            auto const ticks{calculate_tick_count(options.warmup_seconds, tick_rate_hz)};
            if (!ticks) {
                return std::unexpected{"invalid warm-up duration: " + ticks.error()};
            }
            warmup_ticks = *ticks;
        }
        auto const timeout_ticks{
            calculate_tick_count(options.saturation_timeout_seconds, tick_rate_hz)};
        if (!timeout_ticks) {
            return std::unexpected{"invalid saturation timeout: " + timeout_ticks.error()};
        }
        saturation_timeout_ticks = *timeout_ticks;
    }

    if (options.profiler_connection_timeout_seconds.has_value()) {
        if (!sim::profiling::available) {
            return std::unexpected{"profiler support is not enabled in this benchmark build"};
        }

        if (profiler_ready != nullptr) {
            profiler_ready();
        }

        auto const timeout{
            std::chrono::duration<double>{*options.profiler_connection_timeout_seconds}};
        auto const deadline{std::chrono::steady_clock::now() + timeout};
        while (!sim::profiling::is_connected()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return std::unexpected{"timed out waiting for a profiler connection"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

#if defined(IOJ_WITH_TRACY)
    ZoneScopedN("Fighter simulation benchmark");
    if (options.fighter_stress_cap.has_value()) {
        ZoneNameF("Fighter simulation benchmark: %d fighters", *options.fighter_stress_cap);
    }
#endif

    auto reference{sim::make_reference_level_simulation_data()};
    auto& data{reference.data};
    sim::apply_settings(settings, data);
    reference.player.config = settings.player;
    data.clock_settings.time_scale = PositiveFinite<double>::make(1.0);
    if (options.fighter_stress_cap.has_value()) {
        data.fighters.max_live_fighters = *options.fighter_stress_cap;
        data.fighters.laser.damage = 0;
        data.fighters.health = std::numeric_limits<sim::Health>::max() / 2;
        data.capital_ships.max_health = std::numeric_limits<sim::Health>::max() / 2;
    }

    if (level_components.player_entity_id().empty()) {
        data.player.reset();
    } else {
        auto const player_definition{std::ranges::find(level_components.entities(),
                                                       level_components.player_entity_id(),
                                                       &levels::EntitySpawnDefinition::id)};
        auto player{reference.player};
        sim::apply_settings(settings, player);
        player.team = player_definition->team;
        if (player_definition->overrides.health) {
            auto const health{*player_definition->overrides.health};
            player.health = {health, health};
        }
        player.transform.location = player_definition->position;
        player.transform.rotation = to_quaternion(player_definition->rotation);
        data.player = player;
    }

    sim::SimClock compilation_clock;
    compilation_clock.initialise(data.clock_settings);
    auto compiled{
        levels::compile_level(level, compilation_clock, data.capital_ships, data.turrets)};
    if (!compiled) {
        std::string output;
        auto write{ioj::FormatWriter{std::back_inserter(output)}};
        for (auto const& error : compiled.error()) {
            write("{}{}", output.empty() ? "" : "\n", error);
        }
        return std::unexpected{std::move(output)};
    }
    data.level_events = std::move(compiled->events);
    data.mission = std::move(compiled->mission);
    data.result_metadata = std::move(compiled->result_metadata);

    if (options.fighter_stress_cap.has_value()) {
        auto const team_count{data.level_events.schedule
                                  .get_participating_teams(
                                      data.player ? std::optional{data.player->team} : std::nullopt)
                                  .num()};
        if (team_count == 0 || *options.fighter_stress_cap % team_count != 0) {
            return std::unexpected{
                "fighter stress cap must divide evenly across participating teams"};
        }
    }

    if (options.telemetry_enabled) {
        data.telemetry_metadata = sim::LevelTelemetryRunMetadata{
            .level_id = level_components.metadata().id.value,
            .level_display_name = level_components.metadata().title,
            .requested_duration_seconds = Duration::make(options.simulated_seconds),
        };
    }

    sim::GameMemory simulation_memory;
    sim::LevelSim simulation{simulation_memory, std::move(data)};

    simulation.finish_initialisation();
    auto const initial_capital_ships{simulation.get_capital_ships().get_num_instances()};
    auto const initial_turrets{simulation.get_turrets().get_num_instances()};
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
    sim::SimTick saturation_ticks{};
    if (options.fighter_stress_cap.has_value()) {
        auto const cap{static_cast<std::uint32_t>(*options.fighter_stress_cap)};
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
                std::format("fighter population did not reach configured cap {} within {} "
                            "simulated seconds; reached {}",
                            cap,
                            options.saturation_timeout_seconds,
                            saturated_count)};
        }

        auto const spawned_at_saturation{simulation.get_capital_ships().get_fighters_spawned()};
        for (sim::SimTick tick{}; tick < warmup_ticks; ++tick) {
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
    {
        SANDBOX_PROFILE_SCOPE("Benchmark measured ticks");
        for (sim::SimTick tick{}; tick < *requested_ticks; ++tick) {
            auto const tick_started_at{std::chrono::steady_clock::now()};
            auto const advanced{advance_one_tick()};
            if (!advanced) {
                return std::unexpected{advanced.error()};
            }
            auto const tick_finished_at{std::chrono::steady_clock::now()};
            tick_microseconds.push_back(
                std::chrono::duration<double, std::micro>{tick_finished_at - tick_started_at}
                    .count());
            frame_summary.record_tick(simulation);

            auto const fighter_count{simulation.get_fighters().get_num_instances()};
            minimum_measured_fighters = std::min(minimum_measured_fighters, fighter_count);
            maximum_measured_fighters = std::max(maximum_measured_fighters, fighter_count);
            if (options.fighter_stress_cap.has_value() &&
                fighter_count != static_cast<std::uint32_t>(*options.fighter_stress_cap)) {
                return std::unexpected{"fighter population changed during measured ticks"};
            }
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
        simulation.stop_run(sim::LevelTelemetryRunEndReason::DurationReached);
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
    auto const count_task = [fighter_tasks](sim::FighterTask const task) {
        return static_cast<std::uint32_t>(std::ranges::count(fighter_tasks, task));
    };
    return BenchmarkResult{
        .level_path = options.level_path.generic_string(),
        .level_id = level_components.metadata().id.value,
        .level_title = level_components.metadata().title,
        .requested_seconds = options.simulated_seconds,
        .tick_rate_hz = simulation.get_clock().get_tick_rate().get_hz(),
        .game_speed = options.game_speed,
        .requested_ticks = *requested_ticks,
        .completed_ticks = *requested_ticks,
        .advance_calls = advance_calls,
        .completed_seconds = static_cast<double>(*requested_ticks) / tick_rate_hz.get_hz(),
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
        .standby_fighters = count_task(sim::FighterTask::Standby),
        .moving_fighters = count_task(sim::FighterTask::MoveToDestination),
        .attacking_fighters = count_task(sim::FighterTask::Attack),
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
        .active_objectives = simulation.get_mission().active_objectives().size(),
        .inactive_objectives = simulation.get_mission().inactive_objectives().size(),
        .completed_objectives = simulation.get_mission().completed_objectives().size(),
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
        .compiler = IOJ_BENCHMARK_COMPILER_ID,
        .build_type = IOJ_BENCHMARK_BUILD_TYPE,
#ifdef IOJ_BENCHMARK_WITH_TRACY
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

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write(R"({{"schema_version":2,"level":{{"path":{},"id":{},"title":{}}})",
          nlohmann::json(result.level_path).dump(),
          nlohmann::json(result.level_id).dump(),
          nlohmann::json(result.level_title).dump());
    write(R"(,"workload":{{"requested_seconds":{:.17g},"tick_rate_hz":{:.17g},"game_speed":{},)"
          R"("requested_ticks":{},"completed_ticks":{},"completed_seconds":{:.17g},)"
          R"("advance_calls":{},"saturation_ticks":{},"warmup_ticks":{},)"
          R"("measured_ticks":{},"total_simulation_ticks":{}}})",
          result.requested_seconds,
          result.tick_rate_hz,
          result.game_speed,
          result.requested_ticks,
          result.completed_ticks,
          result.completed_seconds,
          result.advance_calls,
          result.saturation_ticks,
          result.warmup_ticks,
          result.measured_ticks,
          result.total_simulation_ticks);
    write(R"(,"timing":{{"elapsed_seconds":{:.17g},"total_elapsed_seconds":{:.17g},)"
          R"("ticks_per_second":{:.17g},"realtime_factor":{:.17g},)"
          R"("mean_tick_microseconds":{:.17g},"median_tick_microseconds":{:.17g},)"
          R"("p95_tick_microseconds":{:.17g},"p99_tick_microseconds":{:.17g}}})",
          result.elapsed_seconds,
          result.total_elapsed_seconds,
          ticks_per_second,
          realtime_factor,
          mean_tick_microseconds,
          result.median_tick_microseconds,
          result.p95_tick_microseconds,
          result.p99_tick_microseconds);
    write(R"(,"fighter_stress":{{"enabled":{},"configured_cap":{},"steady_state_fighters":{},)"
          R"("minimum_measured_fighters":{},"maximum_measured_fighters":{},)"
          R"("fighter_spawns_during_measurement":{},"lasers_spawned_during_measurement":{},)"
          R"("task_counts":{{"standby":{},"move_to_destination":{},"attack":{}}}}})",
          result.fighter_stress_enabled,
          result.configured_fighter_cap,
          result.steady_state_fighters,
          result.minimum_measured_fighters,
          result.maximum_measured_fighters,
          result.fighter_spawns_during_measurement,
          result.lasers_spawned_during_measurement,
          result.standby_fighters,
          result.moving_fighters,
          result.attacking_fighters);
    write(
        R"(,"final_state":{{"active_objectives":{},"inactive_objectives":{},"completed_objectives":{},"initial_capital_ships":{},"initial_turrets":{},)"
        R"("alive_entities":{},"capital_ships":{},"fighters":{},"turrets":{},"spinners":{},)"
        R"("active_lasers":{},"lasers_spawned":{},"peak_fighters":{}}})",
        result.active_objectives,
        result.inactive_objectives,
        result.completed_objectives,
        result.initial_capital_ships,
        result.initial_turrets,
        result.alive_entities,
        result.capital_ships,
        result.fighters,
        result.turrets,
        result.spinners,
        result.active_lasers,
        result.lasers_spawned,
        result.peak_fighters);
    write(R"(,"memory":{{"frame_capacity_bytes":{},"frame_peak_claimed_bytes":{},)"
          R"("frame_peak_payload_bytes":{},"frame_total_padding_bytes":{},)"
          R"("frame_total_root_claims":{},"frame_overflow_count":{}}})",
          result.frame_memory_capacity_bytes,
          result.frame_memory_peak_claimed_bytes,
          result.frame_memory_peak_payload_bytes,
          result.frame_memory_total_padding_bytes,
          result.frame_memory_total_root_claims,
          result.frame_memory_overflow_count);
    write(R"(,"telemetry":{{"enabled":{},"rows":{},"acquired_blocks":{},)"
          R"("retained_blocks":{},"allocated_bytes":{}}})",
          result.telemetry_enabled,
          result.telemetry_rows,
          result.telemetry_acquired_blocks,
          result.telemetry_retained_blocks,
          result.telemetry_allocated_bytes);
    write(
        R"(,"environment":{{"hardware_threads":{},"compiler":{},"build_type":{},"tracy_enabled":{}}}}})",
        result.hardware_threads,
        nlohmann::json(result.compiler).dump(),
        nlohmann::json(result.build_type).dump(),
        result.tracy_enabled);
    return output;
}
} // namespace ioj::simulation_benchmark
