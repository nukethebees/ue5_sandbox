#include <ioj/sim/telemetry/json_validation.h>

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <type_traits>

namespace ioj::sim::telemetry {
namespace {

auto error_at(std::string_view const path, std::string_view const detail) -> std::string {
    return std::format("{}: {}", path, detail);
}

template <typename Integer>
auto parse_json_integer(double const value, std::string_view const path)
    -> std::expected<Integer, std::string> {
    static_assert(std::is_integral_v<Integer>);

    auto const minimum{static_cast<double>(std::numeric_limits<Integer>::lowest())};
    auto const maximum_exclusive{std::ldexp(1.0, std::numeric_limits<Integer>::digits)};
    if (!std::isfinite(value) || std::trunc(value) != value || value < minimum ||
        value >= maximum_exclusive) {
        return std::unexpected{error_at(path, "number is outside the required integer range")};
    }
    if constexpr (std::numeric_limits<Integer>::digits > 53) {
        constexpr auto exact_json_integer_limit{static_cast<double>(max_exact_json_integer)};
        if (value < -exact_json_integer_limit || value > exact_json_integer_limit) {
            return std::unexpected{error_at(path, "integer cannot be represented exactly in JSON")};
        }
    }
    return static_cast<Integer>(value);
}

template <std::integral Integer>
using JsonCount =
    ConstrainedValue<Integer,
                     ConstrainedValuePolicy{
                         [](Integer const value) {
                             return NonNegative<Integer>::valid(value) &&
                                    static_cast<std::uint64_t>(value) <= max_exact_json_integer;
                         },
                         "Count must be nonnegative and exactly representable in JSON"}>;

template <std::integral Integer>
auto validate_json_count(Integer const value, std::string_view const path)
    -> std::expected<void, std::string> {
    if (!JsonCount<Integer>::valid(value)) {
        return std::unexpected{
            error_at(path, "count must be nonnegative and exactly representable in JSON")};
    }
    return {};
}

auto validate_json_count(double const value, std::string_view const path)
    -> std::expected<void, std::string> {
    if (!NonNegativeFinite<double>::valid(value)) {
        return std::unexpected{error_at(path, "value must be finite and nonnegative")};
    }
    return {};
}

template <typename Counts>
auto validate_counts(Counts const& source, std::string_view const path)
    -> std::expected<void, std::string> {
    std::int32_t index{};
    for (auto const& row : source) {
        for (auto const value : row) {
            auto const value_path{std::format("{}[{}]", path, index++)};
            auto const valid{validate_json_count(value, value_path)};
            if (!valid) {
                return valid;
            }
        }
    }
    return {};
}

template <typename Series>
auto validate_series(Series const& source, std::string_view const path)
    -> std::expected<void, std::string> {
    auto const count{source.num()};
    SimTick::value_type previous_tick{};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const tick_path{std::format("{}.ticks[{}]", path, index)};
        auto const tick{source.time_at(index)};
        auto const valid_tick{validate_json_count(tick, tick_path)};
        if (!valid_tick) {
            return valid_tick;
        }
        if (index > 0 && tick <= previous_tick) {
            return std::unexpected{
                error_at(tick_path, "tick coordinates must be strictly increasing")};
        }
        auto const value_path{std::format("{}.values[{}]", path, index)};
        auto const valid_value{validate_json_count(source.value_at(index), value_path)};
        if (!valid_value) {
            return valid_value;
        }
        previous_tick = tick;
    }
    return {};
}

} // namespace

auto parse_json_int32(double const value, std::string_view const path)
    -> std::expected<std::int32_t, std::string> {
    return parse_json_integer<std::int32_t>(value, path);
}

auto parse_json_uint64(double const value, std::string_view const path)
    -> std::expected<std::uint64_t, std::string> {
    return parse_json_integer<std::uint64_t>(value, path);
}

auto validate_level_telemetry_json_record(LevelTelemetryRunRecord const& record)
    -> std::expected<void, std::string> {
    auto const completed_ticks{validate_json_count(record.completion.completed_ticks.value(),
                                                   "completion.completed_ticks")};
    if (!completed_ticks) {
        return completed_ticks;
    }
    for (auto const& valid : std::array{
             validate_series(record.tick_series.active_entities, "tick_series.active_entities"),
             validate_series(record.tick_series.spawned_entities, "tick_series.spawned_entities"),
             validate_series(record.tick_series.destroyed_entities,
                             "tick_series.destroyed_entities"),
             validate_series(record.tick_series.kills, "tick_series.kills"),
             validate_series(record.tick_series.active_lasers, "tick_series.active_lasers"),
             validate_series(record.tick_series.lasers_fired, "tick_series.lasers_fired")}) {
        if (!valid) {
            return valid;
        }
    }

    for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
        auto const valid{
            validate_series(record.tick_series.active_entities_by_type[type],
                            std::format("tick_series.active_entities_by_type[{}]", type))};
        if (!valid) {
            return valid;
        }
    }
    for (std::int32_t team{}; team < LevelTelemetryTickSeries::team_count; ++team) {
        for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
            auto const valid{validate_series(
                record.tick_series.active_entities_by_team_and_type[team][type],
                std::format("tick_series.active_entities_by_team_and_type[{}][{}]", team, type))};
            if (!valid) {
                return valid;
            }
        }
    }
    auto const sample_count{static_cast<std::uint32_t>(record.battle_samples.size())};
    for (std::uint32_t index{}; index < sample_count; ++index) {
        auto const& sample{record.battle_samples[index]};
        auto const path{std::format("battle_samples[{}]", index)};
        auto const& cbt{sample.combat};
        auto const counts{[&path](auto const& values, std::string_view const name) {
            return validate_counts(values, std::format("{}.{}", path, name));
        }};

        for (auto const& valid : std::array{
                 validate_json_count(sample.completed_tick.value(), path + ".completed_tick"),
                 counts(sample.alive, "alive"),
                 counts(cbt.spawned, "combat.spawned"),
                 counts(cbt.destroyed, "combat.destroyed"),
                 counts(cbt.shots, "combat.shots"),
                 counts(cbt.hits, "combat.hits"),
                 counts(cbt.damage_dealt, "combat.damage_dealt"),
                 counts(cbt.damage_received, "combat.damage_received"),
                 counts(cbt.kills, "combat.kills"),
                 counts(cbt.losses, "combat.losses"),
                 counts(cbt.kill_matrix, "combat.kill_matrix"),
                 validate_json_count(sample.active_lasers, path + ".active_lasers"),
                 validate_json_count(sample.lasers_fired, path + ".lasers_fired")}) {
            if (!valid) {
                return valid;
            }
        }
    }
    return {};
}

} // namespace ioj::sim::telemetry
