#include <ioj/sim/telemetry/level_telemetry_json_validation.h>

#include <cmath>
#include <limits>
#include <type_traits>

namespace ioj::sim::telemetry {
namespace {

constexpr double exact_json_integer_limit{9007199254740992.0};

auto error_at(std::string_view const path, std::string_view const detail) -> std::string {
    return std::string{path} + ": " + std::string{detail};
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
    if (!std::isfinite(value)) {
        return std::unexpected{error_at(path, "value must be finite")};
    }
    if (nonnegative && value < 0.0) {
        return std::unexpected{error_at(path, "value must be nonnegative")};
    }
    return {};
}

template <typename Counts>
auto validate_counts(Counts const& source, std::string_view const path)
    -> std::expected<void, std::string> {
    std::int32_t index{};
    for (auto const& row : source) {
        for (auto const value : row) {
            auto const value_path{std::string{path} + "[" + std::to_string(index++) + "]"};
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
    for (std::int32_t index{}; index < count; ++index) {
        auto const tick_path{std::string{path} + ".ticks[" + std::to_string(index) + "]"};
        auto const tick{source.time_at(index)};
        auto const valid_tick{validate_json_integer(tick, tick_path, true)};
        if (!valid_tick) {
            return valid_tick;
        }
        if (index > 0 && tick <= previous_tick) {
            return std::unexpected{
                error_at(tick_path, "tick coordinates must be strictly increasing")};
        }
        auto const value_path{std::string{path} + ".values[" + std::to_string(index) + "]"};
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
    auto const tick_rate{
        validate_json_number(record.metadata.tick_rate_hz, "simulation.tick_rate_hz", false)};
    auto const tick_period{validate_json_number(
        record.metadata.tick_period_seconds, "simulation.tick_period_seconds", false)};
    auto const time_scale{validate_json_number(record.metadata.initial_requested_time_scale,
                                               "simulation.initial_requested_time_scale",
                                               true)};
    auto const sample_interval{validate_json_number(record.metadata.battle_sample_interval_seconds,
                                                    "simulation.battle_sample_interval_seconds",
                                                    false)};
    if (!tick_rate || !tick_period || !time_scale || !sample_interval) {
        return std::unexpected{!tick_rate     ? tick_rate.error()
                               : !tick_period ? tick_period.error()
                               : !time_scale  ? time_scale.error()
                                              : sample_interval.error()};
    }
    if (record.metadata.tick_rate_hz <= 0.0 || record.metadata.tick_period_seconds <= 0.0) {
        return std::unexpected{"simulation rates and period are outside their valid ranges"};
    }
    if (record.metadata.requested_duration_seconds.has_value()) {
        auto const duration{validate_json_number(*record.metadata.requested_duration_seconds,
                                                 "simulation.requested_duration_seconds",
                                                 true)};
        if (!duration) {
            return duration;
        }
    }

    auto const mission_values{record.completion.mission_mode.has_value()};
    if (mission_values != record.completion.mission_state.has_value() ||
        mission_values != record.completion.mission_fail_reason.has_value()) {
        return std::unexpected{
            "completion mission mode, state, and fail reason must be provided together"};
    }
    auto const completed_ticks{validate_json_integer(
        record.completion.completed_ticks, "completion.completed_ticks", true)};
    auto const completion_elapsed{validate_json_number(
        record.completion.simulated_elapsed_seconds, "completion.simulated_elapsed_seconds", true)};
    if (!completed_ticks || !completion_elapsed) {
        return std::unexpected{!completed_ticks ? completed_ticks.error()
                                                : completion_elapsed.error()};
    }
    if (record.completion.mission_elapsed_seconds.has_value()) {
        auto const mission_elapsed{validate_json_number(*record.completion.mission_elapsed_seconds,
                                                        "completion.mission_elapsed_seconds",
                                                        true)};
        if (!mission_elapsed) {
            return mission_elapsed;
        }
    }

#define VALIDATE_SERIES(field)                                                              \
    do {                                                                                    \
        auto const valid{validate_series(record.tick_series.field, "tick_series." #field)}; \
        if (!valid) {                                                                       \
            return valid;                                                                   \
        }                                                                                   \
    } while (false)
    VALIDATE_SERIES(active_entities);
    VALIDATE_SERIES(spawned_entities);
    VALIDATE_SERIES(destroyed_entities);
    VALIDATE_SERIES(kills);
    VALIDATE_SERIES(active_lasers);
    VALIDATE_SERIES(lasers_fired);
#undef VALIDATE_SERIES
    for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
        auto const valid{
            validate_series(record.tick_series.active_entities_by_type[type],
                            "tick_series.active_entities_by_type[" + std::to_string(type) + "]")};
        if (!valid) {
            return valid;
        }
    }
    for (std::int32_t team{}; team < LevelTelemetryTickSeries::team_count; ++team) {
        for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
            auto const valid{
                validate_series(record.tick_series.active_entities_by_team_and_type[team][type],
                                "tick_series.active_entities_by_team_and_type[" +
                                    std::to_string(team) + "][" + std::to_string(type) + "]")};
            if (!valid) {
                return valid;
            }
        }
    }
    for (std::int32_t index{}; index < static_cast<std::int32_t>(record.battle_samples.size());
         ++index) {
        auto const& sample{record.battle_samples[index]};
        auto const path{"battle_samples[" + std::to_string(index) + "]"};
        auto const tick{
            validate_json_integer(sample.completed_tick, path + ".completed_tick", true)};
        auto const elapsed{validate_json_number(
            sample.simulated_elapsed_seconds, path + ".simulated_elapsed_seconds", true)};
        auto const alive{validate_counts(sample.alive, path + ".alive")};
        auto const spawned{validate_counts(sample.combat.spawned, path + ".combat.spawned")};
        auto const destroyed{validate_counts(sample.combat.destroyed, path + ".combat.destroyed")};
        auto const shots{validate_counts(sample.combat.shots, path + ".combat.shots")};
        auto const hits{validate_counts(sample.combat.hits, path + ".combat.hits")};
        auto const damage_dealt{
            validate_counts(sample.combat.damage_dealt, path + ".combat.damage_dealt")};
        auto const damage_received{
            validate_counts(sample.combat.damage_received, path + ".combat.damage_received")};
        auto const kills{validate_counts(sample.combat.kills, path + ".combat.kills")};
        auto const losses{validate_counts(sample.combat.losses, path + ".combat.losses")};
        auto const kill_matrix{
            validate_counts(sample.combat.kill_matrix, path + ".combat.kill_matrix")};
        auto const active_lasers{
            validate_json_integer(sample.active_lasers, path + ".active_lasers", true)};
        auto const lasers_fired{
            validate_json_integer(sample.lasers_fired, path + ".lasers_fired", true)};
        if (!tick || !elapsed || !alive || !spawned || !destroyed || !shots || !hits ||
            !damage_dealt || !damage_received || !kills || !losses || !kill_matrix ||
            !active_lasers || !lasers_fired) {
            return std::unexpected{!tick              ? tick.error()
                                   : !elapsed         ? elapsed.error()
                                   : !alive           ? alive.error()
                                   : !spawned         ? spawned.error()
                                   : !destroyed       ? destroyed.error()
                                   : !shots           ? shots.error()
                                   : !hits            ? hits.error()
                                   : !damage_dealt    ? damage_dealt.error()
                                   : !damage_received ? damage_received.error()
                                   : !kills           ? kills.error()
                                   : !losses          ? losses.error()
                                   : !kill_matrix     ? kill_matrix.error()
                                   : !active_lasers   ? active_lasers.error()
                                                      : lasers_fired.error()};
        }
    }
    return {};
}

} // namespace ioj::sim::telemetry
