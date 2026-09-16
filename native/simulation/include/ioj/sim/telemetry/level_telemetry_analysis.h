#pragma once

#include <ioj/sim/telemetry/level_telemetry_run_record.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim::telemetry {
enum class Metric : std::uint8_t {
    ActiveEntities,
    PlayerShips,
    Turrets,
    CapitalShips,
    CapitalShipFighters,
    TubeSpinners,
    ActiveLasers,
    SpawnRate,
    DestructionRate,
    KillRate,
    LaserFireRate,
    Count,
};

struct AnalysisInput {
    std::int32_t loaded_schema_version{};
    LevelTelemetryRunMetadata const& metadata;
    LevelTelemetryRunCompletion const& completion;
    LevelTelemetryTickSeries const& tick_series;
    std::span<LevelTelemetryBattleSample const> battle_samples;
};

struct MetricSeries {
    Metric metric{Metric::ActiveEntities};
    std::vector<float> simulated_elapsed_seconds;
    std::vector<float> values;
    std::optional<double> weighted_mean;
};

struct Analysis {
    std::vector<MetricSeries> metrics;
    std::vector<float> battle_simulated_seconds;
    std::vector<float> battle_alive_entities;
    std::vector<float> battle_shots;
    std::vector<float> battle_hits;
    std::vector<float> battle_damage_dealt;
    std::vector<float> battle_kills;
};

[[nodiscard]] auto analyze(AnalysisInput input) -> Analysis;
} // namespace ioj::sim::telemetry
