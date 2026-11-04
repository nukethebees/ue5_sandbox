#include <ioj/sim/telemetry/report_json.h>

#include <ioj/sim/telemetry/json_validation.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <format>
#include <limits>
#include <type_traits>
#include <utility>

namespace ioj::sim::telemetry {
namespace {
using Json = nlohmann::json;

struct ReportParseError {
    std::string message;
};

[[noreturn]] void fail(std::string_view const path, std::string_view const detail) {
    throw ReportParseError{std::format("{}: {}", path, detail)};
}

auto field_path(std::string_view const path, std::string_view const field) -> std::string {
    return path.empty() ? std::string{field} : std::format("{}.{}", path, field);
}

auto required_field(Json const& object, std::string_view const field, std::string_view const path)
    -> Json const& {
    if (!object.is_object()) {
        fail(path, "expected an object");
    }
    auto const found{object.find(field)};
    if (found == object.end()) {
        fail(field_path(path, field), "required field is missing");
    }
    return *found;
}

auto required_object(Json const& parent, std::string_view const field, std::string_view const path)
    -> Json const& {
    auto const& value{required_field(parent, field, path)};
    if (!value.is_object()) {
        fail(field_path(path, field), "expected an object");
    }
    return value;
}

auto required_array(Json const& parent, std::string_view const field, std::string_view const path)
    -> Json const& {
    auto const& value{required_field(parent, field, path)};
    if (!value.is_array()) {
        fail(field_path(path, field), "expected an array");
    }
    return value;
}

template <typename Value>
auto read_value(Json const& value, std::string_view const path) -> Value {
    if constexpr (std::is_same_v<Value, SimTick>) {
        return SimTick{read_value<SimTick::value_type>(value, path)};
    } else if constexpr (std::is_same_v<Value, std::string>) {
        if (!value.is_string()) {
            fail(path, "expected a string");
        }
        return value.get<std::string>();
    } else if constexpr (std::is_same_v<Value, bool>) {
        if (!value.is_boolean()) {
            fail(path, "expected a boolean");
        }
        return value.get<bool>();
    } else {
        if (!value.is_number()) {
            fail(path, "expected a number");
        }
        auto const checked{NonNegativeFinite<double>::try_make(value.get<double>())};
        if (!checked) {
            fail(path, "number must be finite and nonnegative");
        }
        auto const number{checked->get()};
        if constexpr (std::is_integral_v<Value>) {
            // Reject precision loss before converting an exact JSON integer to double.
            if (value.is_number_unsigned() && value.get<std::uint64_t>() > max_exact_json_integer) {
                fail(path, "integer cannot be represented exactly in JSON");
            }
            auto const parsed{[&] {
                if constexpr (std::is_same_v<Value, std::int32_t>) {
                    return parse_json_int32(number, path);
                } else {
                    static_assert(std::is_same_v<Value, std::uint64_t>);
                    return parse_json_uint64(number, path);
                }
            }()};
            if (!parsed) {
                throw ReportParseError{parsed.error()};
            }
            return *parsed;
        } else {
            return number;
        }
    }
}

template <typename Value>
auto read_field(Json const& object, std::string_view const field, std::string_view const path)
    -> Value {
    return read_value<Value>(required_field(object, field, path), field_path(path, field));
}

auto has_value(Json const& object, std::string_view const field) -> bool {
    auto const found{object.find(field)};
    return found != object.end() && !found->is_null();
}

auto read_choice(Json const& object,
                 std::string_view const field,
                 std::string_view const path,
                 std::string_view const enabled,
                 std::string_view const disabled) -> bool {
    auto const value{read_field<std::string>(object, field, path)};
    if (value != enabled && value != disabled) {
        fail(field_path(path, field), std::format("unknown value '{}'", value));
    }
    return value == enabled;
}

template <typename Parse>
auto read_enum(Json const& object,
               std::string_view const field,
               std::string_view const path,
               Parse parse) {
    auto const name{read_field<std::string>(object, field, path)};
    auto const value{parse(name)};
    if (!value) {
        fail(field_path(path, field), std::format("unknown enum value '{}'", name));
    }
    return *value;
}

auto read_duration(Json const& object, std::string_view const field, std::string_view const path)
    -> std::optional<Duration> {
    if (!has_value(object, field)) {
        return {};
    }
    auto const value{Duration::try_make(read_field<double>(object, field, path))};
    if (!value) {
        fail(field_path(path, field), "duration must be finite and nonnegative");
    }
    return value;
}

auto write_duration(std::optional<Duration> const value) -> Json {
    return value ? Json(value->seconds()) : Json(nullptr);
}

template <typename Counts>
auto write_counts(Counts const& counts) -> Json {
    auto values = Json::array();
    for (auto const value : counts) {
        values.push_back(value);
    }
    return values;
}

template <typename Counts>
void read_counts(Json const& parent,
                 std::string_view const field,
                 std::string_view const path,
                 Counts& counts) {
    auto const& values{required_array(parent, field, path)};
    auto const location{field_path(path, field)};
    if (values.size() != counts.size()) {
        fail(location, "array has the wrong number of values");
    }
    std::size_t index{};
    for (auto& cell : counts) {
        using Value = std::remove_cvref_t<decltype(cell)>;
        cell = read_value<Value>(values[index], std::format("{}[{}]", location, index));
        ++index;
    }
}

auto write_combat(CombatTelemetryCounters const& source) -> Json {
    return {
        {"spawned", write_counts(source.spawned)},
        {"destroyed", write_counts(source.destroyed)},
        {"shots", write_counts(source.shots)},
        {"hits", write_counts(source.hits)},
        {"damage_dealt", write_counts(source.damage_dealt)},
        {"kills", write_counts(source.kills)},
    };
}

void read_combat(Json const& source, std::string_view const path, CombatTelemetryCounters& output) {
    read_counts(source, "spawned", path, output.spawned);
    read_counts(source, "destroyed", path, output.destroyed);
    read_counts(source, "shots", path, output.shots);
    read_counts(source, "hits", path, output.hits);
    read_counts(source, "damage_dealt", path, output.damage_dealt);
    read_counts(source, "kills", path, output.kills);
}

auto write_sample(BattleSample const& sample) -> Json {
    return {{"completed_tick", sample.completed_tick.value()},
            {"alive", write_counts(sample.alive)},
            {"combat", write_combat(sample.combat)},
            {"active_lasers", sample.active_lasers},
            {"lasers_fired", sample.lasers_fired}};
}

auto read_sample(Json const& source, std::string_view const path) -> BattleSample {
    BattleSample sample;
    sample.completed_tick = read_field<SimTick>(source, "completed_tick", path);
    read_counts(source, "alive", path, sample.alive);
    read_combat(required_object(source, "combat", path), field_path(path, "combat"), sample.combat);
    sample.active_lasers = read_field<std::int32_t>(source, "active_lasers", path);
    sample.lasers_fired = read_field<std::int32_t>(source, "lasers_fired", path);
    return sample;
}

auto validate_report(Report const& report) -> std::expected<void, std::string> {
    if (auto const result{validate_level_telemetry_json_record(report.run)}; !result) {
        return result;
    }
    auto const& completion{report.run.completion};
    auto const reason_count{enum_strings<RunEndReason, Serialized>.offsets.size() - 1};
    if (std::to_underlying(completion.reason) >= reason_count) {
        return std::unexpected{"completion contains an invalid enum value"};
    }
    return {};
}

auto read_report(Json const& root) -> Report {
    auto const schema{read_field<std::int32_t>(root, "schema_version", "")};
    if (schema != RunRecord::schema_version) {
        fail("schema_version", "unsupported report schema");
    }

    Report report;
    report.run.metadata.run_id = read_field<std::string>(root, "run_id", "");
    auto const& level{required_object(root, "level", "")};
    report.run.metadata.map_name = read_field<std::string>(level, "map_name", "level");
    report.run.metadata.level_id = read_field<std::string>(level, "level_id", "level");
    report.run.metadata.level_display_name =
        read_field<std::string>(level, "display_name", "level");
    auto const& timestamps{required_object(root, "timestamps", "")};
    report.run.metadata.launched_utc =
        read_field<std::string>(timestamps, "launched_utc", "timestamps");
    report.run.completion.completed_utc =
        read_field<std::string>(timestamps, "completed_utc", "timestamps");

    auto const& simulation{required_object(root, "simulation", "")};
    auto const frequency{
        Frequency::try_make(read_field<double>(simulation, "tick_rate_hz", "simulation"))};
    if (!frequency) {
        fail("simulation.tick_rate_hz", "frequency must be finite and at least 1e-300 Hz");
    }
    report.run.tick_rate_hz = *frequency;
    auto const read_simulation{[&simulation]<typename T>(T& field, std::string_view const name) {
        field = read_field<T>(simulation, name, "simulation");
    }};
    read_simulation(report.run.metadata.level_source, "level_source");
    read_simulation(report.run.metadata.settings_source, "settings_source");
    report.started_paused =
        read_choice(simulation, "launch_state", "simulation", "paused", "running");
    report.opens_telemetry =
        read_choice(simulation, "results_navigation", "simulation", "telemetry", "none");
    read_simulation(report.presentation_enabled, "presentation_enabled");
    read_simulation(report.run.metadata.stop_when_battle_resolved, "stop_when_battle_resolved");
    report.run.metadata.requested_duration_seconds =
        read_duration(simulation, "requested_duration_seconds", "simulation");

    auto const& completion{required_object(root, "completion", "")};
    auto& result{report.run.completion};
    result.reason =
        read_enum(completion, "reason", "completion", try_parse_serialized_run_end_reason);
    result.completed_ticks = read_field<SimTick>(completion, "completed_ticks", "completion");

    report.run.final_state = read_sample(required_object(root, "final_state", ""), "final_state");
    auto const& samples{required_array(root, "battle_samples", "")};
    report.run.battle_samples.reserve(samples.size());
    auto const count{samples.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& source{samples[index]};
        auto const path{std::format("battle_samples[{}]", index)};
        report.run.battle_samples.push_back(read_sample(source, path));
    }
    return report;
}
}

auto serialize_report(Report const& report) -> std::expected<std::string, std::string> {
    if (auto const valid{validate_report(report)}; !valid) {
        return std::unexpected{valid.error()};
    }
    try {
        auto const& metadata{report.run.metadata};
        auto const& result{report.run.completion};
        Json completion{
            {"reason", to_serialized_string(result.reason)},
            {"completed_ticks", result.completed_ticks.value()},
        };

        auto samples = Json::array();
        for (auto const& sample : report.run.battle_samples) {
            samples.push_back(write_sample(sample));
        }
        Json root{
            {"schema_version", RunRecord::schema_version},
            {"run_id", metadata.run_id},
            {"level",
             {{"map_name", metadata.map_name},
              {"level_id", metadata.level_id},
              {"display_name", metadata.level_display_name}}},
            {"timestamps",
             {{"launched_utc", metadata.launched_utc}, {"completed_utc", result.completed_utc}}},
            {"simulation",
             {{"tick_rate_hz", report.run.tick_rate_hz.hz()},
              {"level_source", metadata.level_source},
              {"settings_source", metadata.settings_source},
              {"launch_state", report.started_paused ? "paused" : "running"},
              {"results_navigation", report.opens_telemetry ? "telemetry" : "none"},
              {"presentation_enabled", report.presentation_enabled},
              {"stop_when_battle_resolved", metadata.stop_when_battle_resolved},
              {"requested_duration_seconds", write_duration(metadata.requested_duration_seconds)}}},
            {"completion", std::move(completion)},
            {"final_state", write_sample(report.run.final_state)},
            {"battle_samples", std::move(samples)},
        };
        return root.dump(2);
    } catch (Json::exception const& error) {
        return std::unexpected{error.what()};
    }
}

auto deserialize_report(std::string_view const json) -> std::expected<Report, std::string> {
    try {
        auto report{read_report(Json::parse(json))};
        if (auto const valid{validate_report(report)}; !valid) {
            return std::unexpected{valid.error()};
        }
        return report;
    } catch (ReportParseError const& error) {
        return std::unexpected{error.message};
    } catch (Json::exception const& error) {
        return std::unexpected{error.what()};
    }
}
}
