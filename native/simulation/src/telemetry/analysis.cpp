#include <ioj/sim/telemetry/analysis.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ioj::sim::telemetry {
auto metric_title(Metric const metric) noexcept -> std::string_view {
    switch (metric) {
        case Metric::ActiveEntities:
            return "Active entities";
        case Metric::PlayerShips:
            return "Active player ships";
        case Metric::Turrets:
            return "Active turrets";
        case Metric::CapitalShips:
            return "Active capital ships";
        case Metric::CapitalShipFighters:
            return "Active capital-ship fighters";
        case Metric::TubeSpinners:
            return "Active tube spinners";
        case Metric::ActiveLasers:
            return "Active lasers";
        case Metric::SpawnRate:
            return "Spawn rate";
        case Metric::DestructionRate:
            return "Destruction rate";
        case Metric::KillRate:
            return "Kill rate";
        case Metric::LaserFireRate:
            return "Laser-fire rate";
    }
    return {};
}
auto metric_units(Metric const metric) noexcept -> std::string_view {
    switch (metric) {
        case Metric::SpawnRate:
        case Metric::DestructionRate:
        case Metric::KillRate:
        case Metric::LaserFireRate:
            return "/s";
        default:
            return "count";
    }
}
namespace {
struct MetricBuilder {
    explicit MetricBuilder(Metric const metric) { series.metric = metric; }

    void add(double const time, double const value, double const weight) {
        if (!std::isfinite(time) || !std::isfinite(value) || !std::isfinite(weight) ||
            weight <= 0.0) {
            return;
        }
        series.simulated_elapsed_seconds.push_back(static_cast<float>(time));
        series.values.push_back(static_cast<float>(value));
        weighted_sum += value * weight;
        total_weight += weight;
    }

    auto finish() -> MetricSeries {
        if (total_weight > 0.0) {
            series.weighted_mean = weighted_sum / total_weight;
        }
        return std::move(series);
    }

    MetricSeries series;
    double weighted_sum{};
    double total_weight{};
};

template <typename Counts>
auto sum_counts(Counts const& counts) -> float {
    double total{};
    for (auto const value : counts) {
        total += static_cast<double>(value);
    }
    return static_cast<float>(total);
}
} // namespace

auto analyze(AnalysisInput const input) -> Analysis {
    Analysis result;
    EnumArray<MetricBuilder, Metric> builders{std::array{MetricBuilder{Metric::ActiveEntities},
                                                         MetricBuilder{Metric::PlayerShips},
                                                         MetricBuilder{Metric::Turrets},
                                                         MetricBuilder{Metric::CapitalShips},
                                                         MetricBuilder{Metric::CapitalShipFighters},
                                                         MetricBuilder{Metric::TubeSpinners},
                                                         MetricBuilder{Metric::ActiveLasers},
                                                         MetricBuilder{Metric::SpawnRate},
                                                         MetricBuilder{Metric::DestructionRate},
                                                         MetricBuilder{Metric::KillRate},
                                                         MetricBuilder{Metric::LaserFireRate}}};
    auto const tick_rate_hz{input.tick_rate_hz.hz()};

    result.battle_simulated_seconds.reserve(input.battle_samples.size());
    result.battle_alive_entities.reserve(input.battle_samples.size());
    result.battle_shots.reserve(input.battle_samples.size());
    result.battle_hits.reserve(input.battle_samples.size());
    result.battle_damage_dealt.reserve(input.battle_samples.size());
    result.battle_kills.reserve(input.battle_samples.size());
    for (auto const& sample : input.battle_samples) {
        result.battle_simulated_seconds.push_back(
            static_cast<float>(static_cast<double>(sample.completed_tick.value()) / tick_rate_hz));
        result.battle_alive_entities.push_back(sum_counts(sample.alive));
        result.battle_shots.push_back(sum_counts(sample.combat.shots));
        result.battle_hits.push_back(sum_counts(sample.combat.hits));
        result.battle_damage_dealt.push_back(sum_counts(sample.combat.damage_dealt));
        result.battle_kills.push_back(sum_counts(sample.combat.kills));
    }

    if (input.battle_samples.size() >= 2) {
        constexpr EnumArray<Metric, EntityType> type_metrics{std::array{Metric::PlayerShips,
                                                                        Metric::Turrets,
                                                                        Metric::CapitalShips,
                                                                        Metric::CapitalShipFighters,
                                                                        Metric::TubeSpinners}};
        auto const type_total{[](EntityCounts const& counts, EntityType const type) {
            std::int32_t total{};
            for (auto const team : EnumTraits<Team>::values) {
                total += counts[team, type];
            }
            return total;
        }};
        auto const sample_count{input.battle_samples.size()};
        for (std::size_t index{1}; index < sample_count; ++index) {
            auto const& begin{input.battle_samples[index - 1]};
            auto const& end{input.battle_samples[index]};
            auto const duration{(static_cast<double>(end.completed_tick.value()) -
                                 static_cast<double>(begin.completed_tick.value())) /
                                tick_rate_hz};
            if (!std::isfinite(duration) || duration <= 0.0) {
                continue;
            }
            auto const time{static_cast<double>(end.completed_tick.value()) / tick_rate_hz};
            builders[Metric::ActiveEntities].add(time, sum_counts(end.alive), duration);
            for (auto const type : EnumTraits<EntityType>::values) {
                builders[type_metrics[type]].add(time, type_total(end.alive, type), duration);
            }
            builders[Metric::ActiveLasers].add(time, end.active_lasers, duration);
            auto const add_battle_rate{
                [&](Metric const metric, double const start, double const finish) {
                    if (finish >= start) {
                        builders[metric].add(time, (finish - start) / duration, duration);
                    }
                }};
            add_battle_rate(Metric::SpawnRate,
                            sum_counts(begin.combat.spawned),
                            sum_counts(end.combat.spawned));
            add_battle_rate(Metric::DestructionRate,
                            sum_counts(begin.combat.destroyed),
                            sum_counts(end.combat.destroyed));
            add_battle_rate(
                Metric::KillRate, sum_counts(begin.combat.kills), sum_counts(end.combat.kills));
            add_battle_rate(Metric::LaserFireRate, begin.lasers_fired, end.lasers_fired);
        }
    }

    result.metrics.reserve(builders.size());
    for (auto& metric : builders) {
        result.metrics.push_back(metric.finish());
    }
    return result;
}
} // namespace ioj::sim::telemetry
