#pragma once

#include "sandbox/core/time_series_data.h"
#include "sandbox/simulation/entity_telemetry.h"
#include "sandbox/simulation/sim_tick.h"

#include <array>
#include <cstdint>

namespace ml::simulation {
struct LevelTelemetryBattleSample {
    SimTick completed_tick{};
    double simulated_elapsed_seconds{};
    telemetry::CombatTelemetryCounters combat{};
    telemetry::EntityCounts alive{};
    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
    std::int32_t registry_slot_count{};
    std::int32_t occupied_spatial_cell_count{};
    std::uint64_t grid_rebuild_count{};
    std::uint64_t range_query_count{};
    std::uint64_t line_trace_count{};
    std::uint64_t sweep_trace_count{};
};

enum class SimulationTelemetryTimingSystem : std::uint8_t {
    Player,
    Capitals,
    Fighters,
    Turrets,
    Spinners,
    Lasers,
    Registry,
    SpatialQueries,
    Mission,
    Telemetry,
    COUNT,
};

enum class LevelTelemetryTimingPhase : std::uint8_t {
    Setup,
    Decision,
    Simulation,
    Resolution,
    End,
    COUNT,
};

struct LevelTelemetryTimingAggregate {
    double mean_ms{};
    double p95_ms{};
    double max_ms{};
    std::uint64_t sample_count{};
};

struct SimulationTelemetryPerformanceWindow {
    static constexpr std::int32_t system_count{
        static_cast<std::int32_t>(SimulationTelemetryTimingSystem::COUNT)};
    static constexpr std::int32_t phase_count{
        static_cast<std::int32_t>(LevelTelemetryTimingPhase::COUNT)};

    double real_elapsed_seconds{};
    SimTick completed_tick{};
    LevelTelemetryTimingAggregate frame{};
    LevelTelemetryTimingAggregate game_thread{};
    LevelTelemetryTimingAggregate simulation_tick{};
    std::array<LevelTelemetryTimingAggregate, system_count> systems{};
    std::array<LevelTelemetryTimingAggregate, phase_count> phases{};
    std::array<double, phase_count> phase_cpu_share{};
};

struct LevelTelemetryTickSeries {
    static constexpr std::int32_t team_count{static_cast<std::int32_t>(telemetry::team_count)};
    static constexpr std::int32_t entity_type_count{
        static_cast<std::int32_t>(telemetry::entity_type_count)};

    using Int32Data = ml::XYSeriesData<SimTick, std::int32_t>;
    using Uint64Data = ml::XYSeriesData<SimTick, std::uint64_t>;
    using DoubleData = ml::XYSeriesData<SimTick, double>;
    using ActiveEntitiesByTypeData = std::array<Int32Data, entity_type_count>;
    using ActiveEntitiesByTeamAndTypeData = std::array<ActiveEntitiesByTypeData, team_count>;

    Int32Data active_entities{};
    ActiveEntitiesByTypeData active_entities_by_type{};
    ActiveEntitiesByTeamAndTypeData active_entities_by_team_and_type{};
    Int32Data spawned_entities{};
    Int32Data destroyed_entities{};
    Int32Data kills{};
    Int32Data registry_slot_count{};
    Int32Data active_lasers{};
    Int32Data lasers_fired{};
    Int32Data occupied_spatial_cell_count{};
    Uint64Data grid_rebuild_count{};
    Uint64Data range_query_count{};
    Uint64Data line_trace_count{};
    Uint64Data sweep_trace_count{};
    DoubleData requested_time_scale{};
};
} // namespace ml::simulation
