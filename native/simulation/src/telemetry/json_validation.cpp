#include <ioj/sim/telemetry/json_validation.h>

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <type_traits>

namespace ioj::sim::telemetry {
namespace {

constexpr double exact_json_integer_limit{9007199254740992.0};

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
        if (value < -exact_json_integer_limit || value > exact_json_integer_limit) {
            return std::unexpected{error_at(path, "integer cannot be represented exactly in JSON")};
        }
    }
    return static_cast<Integer>(value);
}

template <typename Integer>
auto validate_json_integer(Integer const value, std::string_view const path, bool const nonnegative)
    -> std::expected<void, std::string> {
    static_assert(std::is_integral_v<Integer>);

    if constexpr (std::is_signed_v<Integer>) {
        if (nonnegative && value < 0) {
            return std::unexpected{error_at(path, "value must be nonnegative")};
        }
    }
    if constexpr (std::numeric_limits<Integer>::digits > 53) {
        constexpr auto exact_integer_limit{static_cast<Integer>(9007199254740992ULL)};
        if (value > exact_integer_limit) {
            return std::unexpected{error_at(path, "integer cannot be represented exactly in JSON")};
        }
    }
    return {};
}

auto validate_json_number(double const value, std::string_view const path, bool const nonnegative)
    -> std::expected<void, std::string> {
    if (!is_valid_json_number(value, nonnegative)) {
        return std::unexpected{error_at(
            path, !std::isfinite(value) ? "value must be finite" : "value must be nonnegative")};
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
            if constexpr (std::is_integral_v<decltype(value)>) {
                auto const valid{validate_json_integer(value, value_path, true)};
                if (!valid) {
                    return valid;
                }
            } else {
                auto const valid{validate_json_number(value, value_path, true)};
                if (!valid) {
                    return valid;
                }
            }
        }
    }
    return {};
}

template <typename Series>
auto validate_series(Series const& source, std::string_view const path)
    -> std::expected<void, std::string> {
    auto const count{source.num()};
    SimTick previous_tick{};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const tick_path{std::format("{}.ticks[{}]", path, index)};
        auto const tick{source.time_at(index)};
        auto const valid_tick{validate_json_integer(tick, tick_path, true)};
        if (!valid_tick) {
            return valid_tick;
        }
        if (index > 0 && tick <= previous_tick) {
            return std::unexpected{
                error_at(tick_path, "tick coordinates must be strictly increasing")};
        }
        auto const value_path{std::format("{}.values[{}]", path, index)};
        auto const valid_value{validate_json_integer(source.value_at(index), value_path, true)};
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
    auto const mission_values{record.completion.mission_team.has_value()};
    if (mission_values != record.completion.mission_outcome.has_value() ||
        mission_values != record.completion.objective_outcome.has_value() ||
        mission_values == record.completion.decisive_objective_id.empty()) {
        return std::unexpected{"completion mission team, outcome, objective id and objective "
                               "outcome must be provided together"};
    }
    auto const completed_ticks{validate_json_integer(
        record.completion.completed_ticks, "completion.completed_ticks", true)};
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
        for (auto const& valid : std::array{
                 validate_json_integer(sample.completed_tick, path + ".completed_tick", true),
                 validate_counts(sample.alive, path + ".alive"),
                 validate_counts(sample.combat.spawned, path + ".combat.spawned"),
                 validate_counts(sample.combat.destroyed, path + ".combat.destroyed"),
                 validate_counts(sample.combat.shots, path + ".combat.shots"),
                 validate_counts(sample.combat.hits, path + ".combat.hits"),
                 validate_counts(sample.combat.damage_dealt, path + ".combat.damage_dealt"),
                 validate_counts(sample.combat.damage_received, path + ".combat.damage_received"),
                 validate_counts(sample.combat.kills, path + ".combat.kills"),
                 validate_counts(sample.combat.losses, path + ".combat.losses"),
                 validate_counts(sample.combat.kill_matrix, path + ".combat.kill_matrix"),
                 validate_json_integer(sample.active_lasers, path + ".active_lasers", true),
                 validate_json_integer(sample.lasers_fired, path + ".lasers_fired", true)}) {
            if (!valid) {
                return valid;
            }
        }
    }
    return {};
}

} // namespace ioj::sim::telemetry
