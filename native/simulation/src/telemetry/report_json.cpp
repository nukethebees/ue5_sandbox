#include <ioj/sim/telemetry/report_json.h>

#include <ioj/sim/telemetry/json_validation.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <format>
#include <limits>
#include <type_traits>

namespace ioj::sim {
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
    if constexpr (std::is_same_v<Value, std::string>) {
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
        auto const number{value.get<double>()};
        if constexpr (std::is_integral_v<Value>) {
            // Reject precision loss before converting an exact JSON integer to double.
            if (value.is_number_unsigned() &&
                value.get<std::uint64_t>() > telemetry::max_exact_json_integer) {
                fail(path, "integer cannot be represented exactly in JSON");
            }
            auto const parsed{[&] {
                if constexpr (std::is_same_v<Value, std::int32_t>) {
                    return telemetry::parse_json_int32(number, path);
                } else {
                    static_assert(std::is_same_v<Value, std::uint64_t>);
                    return telemetry::parse_json_uint64(number, path);
                }
            }()};
            if (!parsed) {
                throw ReportParseError{parsed.error()};
            }
            return *parsed;
        } else {
            if (!std::isfinite(number)) {
                fail(path, "number must be finite");
            }
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
    return value ? Json(value->get_seconds()) : Json(nullptr);
}

template <typename Counts>
auto write_counts(Counts const& counts) -> Json {
    auto values = Json::array();
    for (auto const& row : counts) {
        for (auto const value : row) {
            values.push_back(value);
        }
    }
    return values;
}

template <typename Counts>
void read_counts(Json const& parent,
                 std::string_view const field,
                 std::string_view const path,
                 Counts& counts) {
    auto const& values{required_array(parent, field, path)};
    std::size_t expected_count{};
    for (auto const& row : counts) {
        expected_count += row.size();
    }
    auto const location{field_path(path, field)};
    if (values.size() != expected_count) {
        fail(location, "array has the wrong number of values");
    }
    std::size_t index{};
    for (auto& row : counts) {
        for (auto& cell : row) {
            using Value = std::remove_cvref_t<decltype(cell)>;
            cell = read_value<Value>(values[index], std::format("{}[{}]", location, index));
            ++index;
        }
    }
}

auto write_series(LevelTelemetryTickSeries::Int32Data const& series) -> Json {
    auto ticks = Json::array();
    auto values = Json::array();
    auto const count{series.num()};
    for (std::uint32_t index{}; index < count; ++index) {
        ticks.push_back(series.time_at(index));
        values.push_back(series.value_at(index));
    }
    return {{"ticks", std::move(ticks)}, {"values", std::move(values)}};
}

void read_series(Json const& parent,
                 std::string_view const field,
                 std::string_view const path,
                 LevelTelemetryTickSeries::Int32Data& series) {
    auto const& object{required_object(parent, field, path)};
    auto const location{field_path(path, field)};
    auto const& ticks{required_array(object, "ticks", location)};
    auto const& values{required_array(object, "values", location)};
    auto const count{ticks.size()};
    if (count != values.size() || count > std::numeric_limits<std::uint32_t>::max()) {
        fail(location, "ticks and values must be aligned and fit in a series");
    }

    series.reserve(static_cast<std::uint32_t>(count));
    SimTick previous{};
    for (std::size_t index{}; index < count; ++index) {
        auto const tick{
            read_value<SimTick>(ticks[index], std::format("{}.ticks[{}]", location, index))};
        if (index > 0 && tick <= previous) {
            fail(location, "tick coordinates must be strictly increasing");
        }
        series.add(
            tick,
            read_value<std::int32_t>(values[index], std::format("{}.values[{}]", location, index)));
        previous = tick;
    }
}

auto write_environment(LevelTelemetryEnvironment const& source) -> Json {
    return {
        {"project_name", source.project_name},
        {"project_version", source.project_version},
        {"engine_version", source.engine_version},
        {"build_version", source.build_version},
        {"build_configuration", source.build_configuration},
        {"execution_mode", source.execution_mode},
        {"world_type", source.world_type},
        {"platform", source.platform},
        {"host_architecture", source.host_architecture},
        {"operating_system_version", source.operating_system_version},
        {"operating_system_subversion", source.operating_system_subversion},
        {"cpu_vendor", source.cpu_vendor},
        {"cpu_brand", source.cpu_brand},
        {"primary_gpu_brand", source.primary_gpu_brand},
        {"physical_core_count", source.physical_core_count},
        {"logical_core_count", source.logical_core_count},
        {"total_physical_memory_bytes", source.total_physical_memory_bytes},
    };
}

auto read_environment(Json const& source) -> LevelTelemetryEnvironment {
    return {
        .project_name = read_field<std::string>(source, "project_name", "environment"),
        .project_version = read_field<std::string>(source, "project_version", "environment"),
        .engine_version = read_field<std::string>(source, "engine_version", "environment"),
        .build_version = read_field<std::string>(source, "build_version", "environment"),
        .build_configuration =
            read_field<std::string>(source, "build_configuration", "environment"),
        .execution_mode = read_field<std::string>(source, "execution_mode", "environment"),
        .world_type = read_field<std::string>(source, "world_type", "environment"),
        .platform = read_field<std::string>(source, "platform", "environment"),
        .host_architecture = read_field<std::string>(source, "host_architecture", "environment"),
        .operating_system_version =
            read_field<std::string>(source, "operating_system_version", "environment"),
        .operating_system_subversion =
            read_field<std::string>(source, "operating_system_subversion", "environment"),
        .cpu_vendor = read_field<std::string>(source, "cpu_vendor", "environment"),
        .cpu_brand = read_field<std::string>(source, "cpu_brand", "environment"),
        .physical_core_count =
            read_field<std::int32_t>(source, "physical_core_count", "environment"),
        .logical_core_count = read_field<std::int32_t>(source, "logical_core_count", "environment"),
        .primary_gpu_brand = read_field<std::string>(source, "primary_gpu_brand", "environment"),
        .total_physical_memory_bytes =
            read_field<std::uint64_t>(source, "total_physical_memory_bytes", "environment"),
    };
}

auto write_combat(telemetry::CombatTelemetryCounters const& source) -> Json {
    return {
        {"spawned", write_counts(source.spawned)},
        {"destroyed", write_counts(source.destroyed)},
        {"shots", write_counts(source.shots)},
        {"hits", write_counts(source.hits)},
        {"damage_dealt", write_counts(source.damage_dealt)},
        {"damage_received", write_counts(source.damage_received)},
        {"kills", write_counts(source.kills)},
        {"losses", write_counts(source.losses)},
        {"kill_matrix", write_counts(source.kill_matrix)},
    };
}

void read_combat(Json const& source,
                 std::string_view const path,
                 telemetry::CombatTelemetryCounters& output) {
    read_counts(source, "spawned", path, output.spawned);
    read_counts(source, "destroyed", path, output.destroyed);
    read_counts(source, "shots", path, output.shots);
    read_counts(source, "hits", path, output.hits);
    read_counts(source, "damage_dealt", path, output.damage_dealt);
    read_counts(source, "damage_received", path, output.damage_received);
    read_counts(source, "kills", path, output.kills);
    read_counts(source, "losses", path, output.losses);
    read_counts(source, "kill_matrix", path, output.kill_matrix);
}

auto write_tick_series(LevelTelemetryTickSeries const& source) -> Json {
    Json result{
        {"active_entities", write_series(source.active_entities)},
        {"spawned_entities", write_series(source.spawned_entities)},
        {"destroyed_entities", write_series(source.destroyed_entities)},
        {"kills", write_series(source.kills)},
        {"active_lasers", write_series(source.active_lasers)},
        {"lasers_fired", write_series(source.lasers_fired)},
    };
    auto by_type = Json::object();
    for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
        by_type[to_serialized_string(static_cast<EntityType>(type))] =
            write_series(source.active_entities_by_type[type]);
    }
    result["active_entities_by_type"] = std::move(by_type);
    auto by_team = Json::object();
    for (std::int32_t team{}; team < LevelTelemetryTickSeries::team_count; ++team) {
        auto row = Json::object();
        for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
            row[to_serialized_string(static_cast<EntityType>(type))] =
                write_series(source.active_entities_by_team_and_type[team][type]);
        }
        by_team[to_serialized_string(static_cast<Team>(team))] = std::move(row);
    }
    result["active_entities_by_team_and_type"] = std::move(by_team);
    return result;
}

void read_tick_series(Json const& source, LevelTelemetryTickSeries& output) {
    read_series(source, "active_entities", "tick_series", output.active_entities);
    read_series(source, "spawned_entities", "tick_series", output.spawned_entities);
    read_series(source, "destroyed_entities", "tick_series", output.destroyed_entities);
    read_series(source, "kills", "tick_series", output.kills);
    read_series(source, "active_lasers", "tick_series", output.active_lasers);
    read_series(source, "lasers_fired", "tick_series", output.lasers_fired);

    auto const& by_type{required_object(source, "active_entities_by_type", "tick_series")};
    for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
        read_series(by_type,
                    to_serialized_string(static_cast<EntityType>(type)),
                    "tick_series.active_entities_by_type",
                    output.active_entities_by_type[type]);
    }
    auto const& by_team{required_object(source, "active_entities_by_team_and_type", "tick_series")};
    for (std::int32_t team{}; team < LevelTelemetryTickSeries::team_count; ++team) {
        auto const name{to_serialized_string(static_cast<Team>(team))};
        auto const path{field_path("tick_series.active_entities_by_team_and_type", name)};
        auto const& row{
            required_object(by_team, name, "tick_series.active_entities_by_team_and_type")};
        for (std::int32_t type{}; type < LevelTelemetryTickSeries::entity_type_count; ++type) {
            read_series(row,
                        to_serialized_string(static_cast<EntityType>(type)),
                        path,
                        output.active_entities_by_team_and_type[team][type]);
        }
    }
}

auto validate_report(LevelTelemetryReport const& report) -> std::expected<void, std::string> {
    if (auto const result{telemetry::validate_level_telemetry_json_record(report)}; !result) {
        return result;
    }
    auto const& environment{report.environment};
    if (environment.physical_core_count < 0 || environment.logical_core_count < 0) {
        return std::unexpected{"environment core counts must be nonnegative"};
    }
    if (environment.total_physical_memory_bytes > telemetry::max_exact_json_integer) {
        return std::unexpected{
            "environment.total_physical_memory_bytes cannot be represented exactly in JSON"};
    }
    auto const& completion{report.completion};
    if (!try_parse_serialized_level_telemetry_run_end_reason(
            to_serialized_string(completion.reason))) {
        return std::unexpected{"completion contains an invalid enum value"};
    }
    return {};
}

auto read_report(Json const& root) -> LevelTelemetryReport {
    auto const schema{read_field<std::int32_t>(root, "schema_version", "")};
    if (schema != LevelTelemetryReport::schema_version) {
        fail("schema_version", "unsupported report schema");
    }

    LevelTelemetryReport report;
    report.metadata.run_id = read_field<std::string>(root, "run_id", "");
    auto const& level{required_object(root, "level", "")};
    report.metadata.map_name = read_field<std::string>(level, "map_name", "level");
    report.metadata.level_id = read_field<std::string>(level, "level_id", "level");
    report.metadata.level_display_name = read_field<std::string>(level, "display_name", "level");
    auto const& timestamps{required_object(root, "timestamps", "")};
    report.metadata.launched_utc =
        read_field<std::string>(timestamps, "launched_utc", "timestamps");
    report.completion.completed_utc =
        read_field<std::string>(timestamps, "completed_utc", "timestamps");
    report.environment = read_environment(required_object(root, "environment", ""));

    auto const& simulation{required_object(root, "simulation", "")};
    auto const frequency{
        Frequency::try_make(read_field<double>(simulation, "tick_rate_hz", "simulation"))};
    if (!frequency) {
        fail("simulation.tick_rate_hz", "frequency must be finite and at least 1e-300 Hz");
    }
    report.tick_rate_hz = *frequency;
    report.metadata.level_source =
        read_field<std::string>(simulation, "level_source", "simulation");
    report.metadata.settings_source =
        read_field<std::string>(simulation, "settings_source", "simulation");
    report.launch_state = read_field<std::string>(simulation, "launch_state", "simulation");
    report.presentation_mode =
        read_field<std::string>(simulation, "presentation_mode", "simulation");
    report.results_navigation =
        read_field<std::string>(simulation, "results_navigation", "simulation");
    report.presentation_enabled =
        read_field<bool>(simulation, "presentation_enabled", "simulation");
    report.metadata.stop_when_battle_resolved =
        read_field<bool>(simulation, "stop_when_battle_resolved", "simulation");
    report.metadata.requested_duration_seconds =
        read_duration(simulation, "requested_duration_seconds", "simulation");

    auto const& completion{required_object(root, "completion", "")};
    auto& result{report.completion};
    result.reason = read_enum(
        completion, "reason", "completion", try_parse_serialized_level_telemetry_run_end_reason);
    result.completed_ticks = read_field<SimTick>(completion, "completed_ticks", "completion");

    read_tick_series(required_object(root, "tick_series", ""), report.tick_series);
    auto const& samples{required_array(root, "battle_samples", "")};
    report.battle_samples.reserve(samples.size());
    auto const count{samples.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& source{samples[index]};
        auto const path{std::format("battle_samples[{}]", index)};
        LevelTelemetryBattleSample sample;
        sample.completed_tick = read_field<SimTick>(source, "completed_tick", path);
        read_counts(source, "alive", path, sample.alive);
        read_combat(required_object(source, "combat", path), path + ".combat", sample.combat);
        if (source.contains("active_lasers")) {
            sample.active_lasers = read_field<std::int32_t>(source, "active_lasers", path);
        }
        if (source.contains("lasers_fired")) {
            sample.lasers_fired = read_field<std::int32_t>(source, "lasers_fired", path);
        }
        report.battle_samples.push_back(std::move(sample));
    }
    return report;
}
}

auto serialize_level_telemetry_report(LevelTelemetryReport const& report)
    -> std::expected<std::string, std::string> {
    if (auto const valid{validate_report(report)}; !valid) {
        return std::unexpected{valid.error()};
    }
    try {
        auto const& metadata{report.metadata};
        auto const& result{report.completion};
        Json completion{
            {"reason", to_serialized_string(result.reason)},
            {"completed_ticks", result.completed_ticks},
        };

        auto samples = Json::array();
        for (auto const& sample : report.battle_samples) {
            samples.push_back({{"completed_tick", sample.completed_tick},
                               {"alive", write_counts(sample.alive)},
                               {"combat", write_combat(sample.combat)},
                               {"active_lasers", sample.active_lasers},
                               {"lasers_fired", sample.lasers_fired}});
        }
        Json root{
            {"schema_version", LevelTelemetryReport::schema_version},
            {"run_id", metadata.run_id},
            {"level",
             {{"map_name", metadata.map_name},
              {"level_id", metadata.level_id},
              {"display_name", metadata.level_display_name}}},
            {"timestamps",
             {{"launched_utc", metadata.launched_utc}, {"completed_utc", result.completed_utc}}},
            {"environment", write_environment(report.environment)},
            {"simulation",
             {{"tick_rate_hz", report.tick_rate_hz.get_hz()},
              {"level_source", metadata.level_source},
              {"settings_source", metadata.settings_source},
              {"launch_state", report.launch_state},
              {"presentation_mode", report.presentation_mode},
              {"results_navigation", report.results_navigation},
              {"presentation_enabled", report.presentation_enabled},
              {"stop_when_battle_resolved", metadata.stop_when_battle_resolved},
              {"requested_duration_seconds", write_duration(metadata.requested_duration_seconds)}}},
            {"completion", std::move(completion)},
            {"tick_series", write_tick_series(report.tick_series)},
            {"battle_samples", std::move(samples)},
        };
        return root.dump(2);
    } catch (Json::exception const& error) {
        return std::unexpected{error.what()};
    }
}

auto deserialize_level_telemetry_report(std::string_view const json)
    -> std::expected<LevelTelemetryReport, std::string> {
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
