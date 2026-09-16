#include <ioj/sim/telemetry/level_telemetry_analysis.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ioj::sim::telemetry {
namespace {
template <typename Series>
auto as_of(Series const& series, std::uint64_t const tick) -> typename Series::value_type const* {
    auto const count{series.num()};
    for (std::int32_t index{count - 1}; index >= 0; --index) {
        if (series.time_at(index) <= tick) {
            return &series.value_at(index);
        }
    }
    return nullptr;
}

struct MetricBuilder {
    MetricSeries series;
    double weighted_sum{};
    double total_weight{};

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
};

template <typename Series>
void add_gauge(MetricBuilder& builder,
               Series const& series,
               std::uint64_t const tick,
               double const time,
               double const weight) {
    if (auto const* const value{as_of(series, tick)}) {
        builder.add(time, static_cast<double>(*value), weight);
    }
}

template <typename Series>
void add_rate(MetricBuilder& builder,
              Series const& series,
              std::uint64_t const begin_tick,
              std::uint64_t const end_tick,
              double const time,
              double const duration) {
    auto const* const begin{as_of(series, begin_tick)};
    auto const* const end{as_of(series, end_tick)};
    if (!begin || !end || *end < *begin) {
        return;
    }
    builder.add(
        time, (static_cast<double>(*end) - static_cast<double>(*begin)) / duration, duration);
}

template <typename Counts>
auto sum_counts(Counts const& counts) -> float {
    double total{};
    for (auto const& row : counts) {
        for (auto const value : row) {
            total += static_cast<double>(value);
        }
    }
    return static_cast<float>(total);
}
} // namespace

auto analyze(AnalysisInput const input) -> Analysis {
    Analysis result;
    std::array<MetricBuilder, static_cast<std::size_t>(Metric::Count)> builders{
        MetricBuilder{Metric::ActiveEntities},
        MetricBuilder{Metric::PlayerShips},
        MetricBuilder{Metric::Turrets},
        MetricBuilder{Metric::CapitalShips},
        MetricBuilder{Metric::CapitalShipFighters},
        MetricBuilder{Metric::TubeSpinners},
        MetricBuilder{Metric::ActiveLasers},
        MetricBuilder{Metric::SpawnRate},
        MetricBuilder{Metric::DestructionRate},
        MetricBuilder{Metric::KillRate},
        MetricBuilder{Metric::LaserFireRate}};
    auto const builder{[&builders](Metric const metric) -> MetricBuilder& {
        return builders[static_cast<std::size_t>(metric)];
    }};
    auto const use_battle_metrics{input.loaded_schema_version >= 2 &&
                                  input.battle_samples.size() >= 2};

    if (!use_battle_metrics && input.metadata.tick_period_seconds > 0.0 &&
        std::isfinite(input.metadata.tick_period_seconds)) {
        std::vector<std::uint64_t> ticks;
        auto const collect_ticks{[&ticks](auto const& series) {
            auto const count{series.num()};
            for (std::int32_t index{}; index < count; ++index) {
                ticks.push_back(series.time_at(index));
            }
        }};
        collect_ticks(input.tick_series.active_entities);
        collect_ticks(input.tick_series.spawned_entities);
        collect_ticks(input.tick_series.destroyed_entities);
        collect_ticks(input.tick_series.kills);
        collect_ticks(input.tick_series.active_lasers);
        collect_ticks(input.tick_series.lasers_fired);
        for (auto const& series : input.tick_series.active_entities_by_type) {
            collect_ticks(series);
        }
        ticks.push_back(input.completion.completed_ticks);
        std::sort(ticks.begin(), ticks.end());

        constexpr std::array type_metrics{Metric::PlayerShips,
                                          Metric::Turrets,
                                          Metric::CapitalShips,
                                          Metric::CapitalShipFighters,
                                          Metric::TubeSpinners};
        for (std::size_t index{1}; index < ticks.size(); ++index) {
            auto const begin_tick{ticks[index - 1]};
            auto const end_tick{ticks[index]};
            if (end_tick == begin_tick) {
                continue;
            }
            auto const duration{static_cast<double>(end_tick - begin_tick) *
                                input.metadata.tick_period_seconds};
            auto const time{static_cast<double>(end_tick) * input.metadata.tick_period_seconds};
            add_gauge(builder(Metric::ActiveEntities),
                      input.tick_series.active_entities,
                      end_tick,
                      time,
                      duration);
            for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
                add_gauge(builder(type_metrics[static_cast<std::size_t>(type)]),
                          input.tick_series.active_entities_by_type[static_cast<std::size_t>(type)],
                          end_tick,
                          time,
                          duration);
            }
            add_gauge(builder(Metric::ActiveLasers),
                      input.tick_series.active_lasers,
                      end_tick,
                      time,
                      duration);
            add_rate(builder(Metric::SpawnRate),
                     input.tick_series.spawned_entities,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(Metric::DestructionRate),
                     input.tick_series.destroyed_entities,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(Metric::KillRate),
                     input.tick_series.kills,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(Metric::LaserFireRate),
                     input.tick_series.lasers_fired,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
        }
    }

    result.battle_simulated_seconds.reserve(input.battle_samples.size());
    result.battle_alive_entities.reserve(input.battle_samples.size());
    result.battle_shots.reserve(input.battle_samples.size());
    result.battle_hits.reserve(input.battle_samples.size());
    result.battle_damage_dealt.reserve(input.battle_samples.size());
    result.battle_kills.reserve(input.battle_samples.size());
    for (auto const& sample : input.battle_samples) {
        result.battle_simulated_seconds.push_back(
            static_cast<float>(sample.simulated_elapsed_seconds));
        result.battle_alive_entities.push_back(sum_counts(sample.alive));
        result.battle_shots.push_back(sum_counts(sample.combat.shots));
        result.battle_hits.push_back(sum_counts(sample.combat.hits));
        result.battle_damage_dealt.push_back(sum_counts(sample.combat.damage_dealt));
        result.battle_kills.push_back(sum_counts(sample.combat.kills));
    }

    if (use_battle_metrics) {
        constexpr std::array type_metrics{Metric::PlayerShips,
                                          Metric::Turrets,
                                          Metric::CapitalShips,
                                          Metric::CapitalShipFighters,
                                          Metric::TubeSpinners};
        auto const type_total{
            [](::ioj::sim::telemetry::EntityCounts const& counts, std::int32_t const type) {
                std::int32_t total{};
                for (auto const& team : counts) {
                    total += team[static_cast<std::size_t>(type)];
                }
                return total;
            }};
        for (std::size_t index{1}; index < input.battle_samples.size(); ++index) {
            auto const& begin{input.battle_samples[index - 1]};
            auto const& end{input.battle_samples[index]};
            auto const duration{end.simulated_elapsed_seconds - begin.simulated_elapsed_seconds};
            if (!std::isfinite(duration) || duration <= 0.0) {
                continue;
            }
            auto const time{end.simulated_elapsed_seconds};
            builder(Metric::ActiveEntities).add(time, sum_counts(end.alive), duration);
            for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
                builder(type_metrics[static_cast<std::size_t>(type)])
                    .add(time, type_total(end.alive, type), duration);
            }
            builder(Metric::ActiveLasers).add(time, end.active_lasers, duration);
            auto const add_battle_rate{
                [&](Metric const metric, double const start, double const finish) {
                    if (finish >= start) {
                        builder(metric).add(time, (finish - start) / duration, duration);
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
