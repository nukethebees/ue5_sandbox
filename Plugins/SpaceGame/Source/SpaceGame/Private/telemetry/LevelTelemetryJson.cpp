#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <SandboxCore/container_ops.h>

#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

namespace {

void set_optional_number(FJsonObject& object, FString const& name, TOptional<double> const value) {
    if (value.IsSet()) {
        object.SetNumberField(name, value.GetValue());
    } else {
        object.SetField(name, MakeShared<FJsonValueNull>());
    }
}

template <typename Series>
auto make_series(Series const& series) -> TSharedRef<FJsonObject> {
    TArray<TSharedPtr<FJsonValue>> ticks;
    TArray<TSharedPtr<FJsonValue>> values;
    auto const count{series.num()};
    ml::reserve(count, ticks, values);
    for (int32 index{}; index < count; ++index) {
        ticks.Add(MakeShared<FJsonValueNumber>(static_cast<double>(series.time_at(index))));
        values.Add(MakeShared<FJsonValueNumber>(static_cast<double>(series.value_at(index))));
    }

    auto result{MakeShared<FJsonObject>()};
    result->SetArrayField(TEXT("ticks"), MoveTemp(ticks));
    result->SetArrayField(TEXT("values"), MoveTemp(values));
    return result;
}

auto make_realtime_series(ml::TimeSeriesData<uint64> const& series) -> TSharedRef<FJsonObject> {
    TArray<TSharedPtr<FJsonValue>> elapsed_seconds;
    TArray<TSharedPtr<FJsonValue>> completed_ticks;
    auto const count{series.num()};
    ml::reserve(count, elapsed_seconds, completed_ticks);
    for (int32 index{}; index < count; ++index) {
        elapsed_seconds.Add(MakeShared<FJsonValueNumber>(series.time_at(index)));
        completed_ticks.Add(
            MakeShared<FJsonValueNumber>(static_cast<double>(series.value_at(index))));
    }

    auto result{MakeShared<FJsonObject>()};
    result->SetArrayField(TEXT("real_elapsed_seconds"), MoveTemp(elapsed_seconds));
    result->SetArrayField(TEXT("completed_ticks"), MoveTemp(completed_ticks));
    return result;
}

auto make_tick_series(FLevelTelemetryTickSeries const& source) -> TSharedRef<FJsonObject> {
    auto result{MakeShared<FJsonObject>()};
    result->SetObjectField(TEXT("active_entities"), make_series(source.active_entities));

    auto active_by_type{MakeShared<FJsonObject>()};
    constexpr auto entity_type_count{FLevelTelemetryTickSeries::entity_type_count};
    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        active_by_type->SetObjectField(
            LexToSerializedString(static_cast<ETestEntityType>(entity_type_index)),
            make_series(source.active_entities_by_type[entity_type_index]));
    }
    result->SetObjectField(TEXT("active_entities_by_type"), active_by_type);

    auto active_by_team_and_type{MakeShared<FJsonObject>()};
    constexpr auto team_count{FLevelTelemetryTickSeries::team_count};
    for (int32 team_index{}; team_index < team_count; ++team_index) {
        auto team{MakeShared<FJsonObject>()};
        for (int32 entity_type_index{}; entity_type_index < entity_type_count;
             ++entity_type_index) {
            team->SetObjectField(
                LexToSerializedString(static_cast<ETestEntityType>(entity_type_index)),
                make_series(
                    source.active_entities_by_team_and_type[team_index][entity_type_index]));
        }
        active_by_team_and_type->SetObjectField(
            LexToSerializedString(static_cast<ETestTeam>(team_index)), team);
    }
    result->SetObjectField(TEXT("active_entities_by_team_and_type"), active_by_team_and_type);

    result->SetObjectField(TEXT("spawned_entities"), make_series(source.spawned_entities));
    result->SetObjectField(TEXT("destroyed_entities"), make_series(source.destroyed_entities));
    result->SetObjectField(TEXT("kills"), make_series(source.kills));
    result->SetObjectField(TEXT("registry_slot_count"), make_series(source.registry_slot_count));
    result->SetObjectField(TEXT("active_lasers"), make_series(source.active_lasers));
    result->SetObjectField(TEXT("lasers_fired"), make_series(source.lasers_fired));
    result->SetObjectField(TEXT("occupied_spatial_cell_count"),
                           make_series(source.occupied_spatial_cell_count));
    result->SetObjectField(TEXT("grid_rebuild_count"), make_series(source.grid_rebuild_count));
    result->SetObjectField(TEXT("range_query_count"), make_series(source.range_query_count));
    result->SetObjectField(TEXT("line_trace_count"), make_series(source.line_trace_count));
    result->SetObjectField(TEXT("sweep_trace_count"), make_series(source.sweep_trace_count));
    result->SetObjectField(TEXT("requested_time_scale"), make_series(source.requested_time_scale));
    return result;
}

template <typename Counts>
auto make_flat_counts(Counts const& source) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> result;
    for (auto const& row : source) {
        for (auto const value : row) {
            result.Add(MakeShared<FJsonValueNumber>(static_cast<double>(value)));
        }
    }
    return result;
}

auto make_timing(FLevelTelemetryTimingAggregate const& source) -> TSharedRef<FJsonObject> {
    auto result{MakeShared<FJsonObject>()};
    result->SetNumberField(TEXT("mean_ms"), source.mean_ms);
    result->SetNumberField(TEXT("p95_ms"), source.p95_ms);
    result->SetNumberField(TEXT("max_ms"), source.max_ms);
    result->SetNumberField(TEXT("sample_count"), static_cast<double>(source.sample_count));
    return result;
}

auto make_combat(FTestEntityRegistry::CombatTelemetryCounters const& source)
    -> TSharedRef<FJsonObject> {
    auto result{MakeShared<FJsonObject>()};
    result->SetArrayField(TEXT("spawned"), make_flat_counts(source.spawned));
    result->SetArrayField(TEXT("destroyed"), make_flat_counts(source.destroyed));
    result->SetArrayField(TEXT("shots"), make_flat_counts(source.shots));
    result->SetArrayField(TEXT("hits"), make_flat_counts(source.hits));
    result->SetArrayField(TEXT("damage_dealt"), make_flat_counts(source.damage_dealt));
    result->SetArrayField(TEXT("damage_received"), make_flat_counts(source.damage_received));
    result->SetArrayField(TEXT("kills"), make_flat_counts(source.kills));
    result->SetArrayField(TEXT("losses"), make_flat_counts(source.losses));
    result->SetArrayField(TEXT("kill_matrix"), make_flat_counts(source.kill_matrix));
    return result;
}

auto make_battle_samples(TArray<FLevelTelemetryBattleSample> const& source)
    -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> result;
    result.Reserve(source.Num());
    for (auto const& sample : source) {
        auto object{MakeShared<FJsonObject>()};
        object->SetNumberField(TEXT("completed_tick"), static_cast<double>(sample.completed_tick));
        object->SetNumberField(TEXT("simulated_elapsed_seconds"), sample.simulated_elapsed_seconds);
        object->SetArrayField(TEXT("alive"), make_flat_counts(sample.alive));
        object->SetObjectField(TEXT("combat"), make_combat(sample.combat));
        object->SetNumberField(TEXT("active_lasers"), sample.active_lasers);
        object->SetNumberField(TEXT("lasers_fired"), sample.lasers_fired);
        object->SetNumberField(TEXT("registry_slot_count"), sample.registry_slot_count);
        object->SetNumberField(TEXT("occupied_spatial_cell_count"),
                               sample.occupied_spatial_cell_count);
        object->SetNumberField(TEXT("grid_rebuild_count"),
                               static_cast<double>(sample.grid_rebuild_count));
        object->SetNumberField(TEXT("range_query_count"),
                               static_cast<double>(sample.range_query_count));
        object->SetNumberField(TEXT("line_trace_count"),
                               static_cast<double>(sample.line_trace_count));
        object->SetNumberField(TEXT("sweep_trace_count"),
                               static_cast<double>(sample.sweep_trace_count));
        result.Add(MakeShared<FJsonValueObject>(object));
    }
    return result;
}

auto make_performance_windows(TArray<FLevelTelemetryPerformanceWindow> const& source)
    -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> result;
    result.Reserve(source.Num());
    for (auto const& window : source) {
        auto object{MakeShared<FJsonObject>()};
        object->SetNumberField(TEXT("real_elapsed_seconds"), window.real_elapsed_seconds);
        object->SetNumberField(TEXT("completed_tick"), static_cast<double>(window.completed_tick));
        object->SetObjectField(TEXT("frame"), make_timing(window.frame));
        object->SetObjectField(TEXT("game_thread"), make_timing(window.game_thread));
        object->SetObjectField(TEXT("render_thread"), make_timing(window.render_thread));
        object->SetObjectField(TEXT("gpu"), make_timing(window.gpu));
        object->SetObjectField(TEXT("simulation_tick"), make_timing(window.simulation_tick));
        TArray<TSharedPtr<FJsonValue>> systems;
        systems.Reserve(FLevelTelemetryPerformanceWindow::system_count);
        for (auto const& timing : window.systems) {
            systems.Add(MakeShared<FJsonValueObject>(make_timing(timing)));
        }
        object->SetArrayField(TEXT("systems"), MoveTemp(systems));
        TArray<TSharedPtr<FJsonValue>> phases;
        TArray<TSharedPtr<FJsonValue>> phase_cpu_share;
        ml::reserve(FLevelTelemetryPerformanceWindow::phase_count, phases, phase_cpu_share);
        for (int32 index{}; index < FLevelTelemetryPerformanceWindow::phase_count; ++index) {
            phases.Add(MakeShared<FJsonValueObject>(make_timing(window.phases[index])));
            phase_cpu_share.Add(MakeShared<FJsonValueNumber>(window.phase_cpu_share[index]));
        }
        object->SetArrayField(TEXT("phases"), MoveTemp(phases));
        object->SetArrayField(TEXT("phase_cpu_share"), MoveTemp(phase_cpu_share));
        result.Add(MakeShared<FJsonValueObject>(object));
    }
    return result;
}

auto error_at(FString const& path, FString const& detail) -> FString {
    return FString::Printf(TEXT("%s: %s"), *path, *detail);
}

auto required_object(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<TSharedPtr<FJsonObject>, FString> {
    TSharedPtr<FJsonObject> const* result{};
    if (!source.TryGetObjectField(field, result) || !result || !result->IsValid()) {
        return std::unexpected{error_at(path, TEXT("required object is missing or invalid"))};
    }
    return *result;
}

auto required_string(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<FString, FString> {
    FString result;
    if (!source.TryGetStringField(field, result)) {
        return std::unexpected{error_at(path, TEXT("required string is missing or invalid"))};
    }
    return result;
}

auto required_bool(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<bool, FString> {
    bool result{};
    if (!source.TryGetBoolField(field, result)) {
        return std::unexpected{error_at(path, TEXT("required boolean is missing or invalid"))};
    }
    return result;
}

auto required_number(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<double, FString> {
    double result{};
    if (!source.TryGetNumberField(field, result) || !FMath::IsFinite(result)) {
        return std::unexpected{
            error_at(path, TEXT("required finite number is missing or invalid"))};
    }
    return result;
}

template <typename Integer>
auto required_integer(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<Integer, FString> {
    auto const number{required_number(source, field, path)};
    if (!number) {
        return std::unexpected{number.error()};
    }
    auto const value{*number};
    auto const minimum{static_cast<double>(std::numeric_limits<Integer>::lowest())};
    auto const maximum{static_cast<double>(std::numeric_limits<Integer>::max())};
    auto const above_maximum{std::is_same_v<Integer, uint64> ? value >= 18446744073709551616.0
                                                             : value > maximum};
    if (FMath::TruncToDouble(value) != value || value < minimum || above_maximum) {
        return std::unexpected{
            error_at(path, TEXT("number is outside the required integer range"))};
    }
    return static_cast<Integer>(value);
}

template <typename Counts>
auto parse_flat_counts(FJsonObject const& source,
                       TCHAR const* const field,
                       FString const& path,
                       Counts& output) -> std::expected<void, FString> {
    TArray<TSharedPtr<FJsonValue>> const* values{};
    if (!source.TryGetArrayField(field, values) || !values) {
        return std::unexpected{error_at(path, TEXT("required array is missing or invalid"))};
    }
    int32 expected_count{};
    for (auto const& row : output) {
        expected_count += row.Num();
    }
    if (values->Num() != expected_count) {
        return std::unexpected{error_at(path, TEXT("array has the wrong number of values"))};
    }
    int32 index{};
    for (auto& row : output) {
        for (auto& cell : row) {
            auto const value{(*values)[index++]->AsNumber()};
            if (!FMath::IsFinite(value) || value < 0.0) {
                return std::unexpected{
                    error_at(path, TEXT("values must be finite and nonnegative"))};
            }
            using Value = std::remove_cvref_t<decltype(cell)>;
            cell = static_cast<Value>(value);
        }
    }
    return {};
}

auto parse_timing(FJsonObject const& source,
                  TCHAR const* const field,
                  FString const& path,
                  FLevelTelemetryTimingAggregate& output) -> std::expected<void, FString> {
    auto const object{required_object(source, field, path)};
    if (!object) {
        return std::unexpected{object.error()};
    }
    auto mean{required_number(**object, TEXT("mean_ms"), path + TEXT(".mean_ms"))};
    auto p95{required_number(**object, TEXT("p95_ms"), path + TEXT(".p95_ms"))};
    auto maximum{required_number(**object, TEXT("max_ms"), path + TEXT(".max_ms"))};
    auto count{
        required_integer<uint64>(**object, TEXT("sample_count"), path + TEXT(".sample_count"))};
    if (!mean || !p95 || !maximum || !count) {
        return std::unexpected{!mean      ? mean.error()
                               : !p95     ? p95.error()
                               : !maximum ? maximum.error()
                                          : count.error()};
    }
    output = {.mean_ms = *mean, .p95_ms = *p95, .max_ms = *maximum, .sample_count = *count};
    return {};
}

auto parse_combat(FJsonObject const& source,
                  FTestEntityRegistry::CombatTelemetryCounters& output,
                  FString const& path) -> std::expected<void, FString> {
#define PARSE_COMBAT_COUNTS(field)                                                           \
    do {                                                                                     \
        auto parsed{                                                                         \
            parse_flat_counts(source, TEXT(#field), path + TEXT("." #field), output.field)}; \
        if (!parsed) {                                                                       \
            return parsed;                                                                   \
        }                                                                                    \
    } while (false)
    PARSE_COMBAT_COUNTS(spawned);
    PARSE_COMBAT_COUNTS(destroyed);
    PARSE_COMBAT_COUNTS(shots);
    PARSE_COMBAT_COUNTS(hits);
    PARSE_COMBAT_COUNTS(damage_dealt);
    PARSE_COMBAT_COUNTS(damage_received);
    PARSE_COMBAT_COUNTS(kills);
    PARSE_COMBAT_COUNTS(losses);
    PARSE_COMBAT_COUNTS(kill_matrix);
#undef PARSE_COMBAT_COUNTS
    return {};
}

template <typename Value>
auto parse_tick_series(FJsonObject const& parent,
                       TCHAR const* const field,
                       FString const& path,
                       ml::XYSeriesData<uint64, Value>& output) -> std::expected<void, FString> {
    auto const object{required_object(parent, field, path)};
    if (!object) {
        return std::unexpected{object.error()};
    }
    TArray<TSharedPtr<FJsonValue>> const* ticks{};
    TArray<TSharedPtr<FJsonValue>> const* values{};
    if (!(*object)->TryGetArrayField(TEXT("ticks"), ticks) ||
        !(*object)->TryGetArrayField(TEXT("values"), values) || !ticks || !values) {
        return std::unexpected{error_at(path, TEXT("ticks and values arrays are required"))};
    }
    if (ticks->Num() != values->Num()) {
        return std::unexpected{error_at(path, TEXT("ticks and values arrays are not aligned"))};
    }

    output.reserve(ticks->Num());
    uint64 previous_tick{};
    for (int32 index{}; index < ticks->Num(); ++index) {
        double tick_number{};
        double value_number{};
        if (!(*ticks)[index].IsValid() || !(*ticks)[index]->TryGetNumber(tick_number) ||
            !FMath::IsFinite(tick_number) || FMath::TruncToDouble(tick_number) != tick_number ||
            tick_number < 0.0 || tick_number >= 18446744073709551616.0) {
            return std::unexpected{error_at(path, TEXT("tick coordinate is not a valid uint64"))};
        }
        auto const tick{static_cast<uint64>(tick_number)};
        if (index > 0 && tick <= previous_tick) {
            return std::unexpected{
                error_at(path, TEXT("tick coordinates must be strictly increasing"))};
        }
        if (!(*values)[index].IsValid() || !(*values)[index]->TryGetNumber(value_number) ||
            !FMath::IsFinite(value_number)) {
            return std::unexpected{error_at(path, TEXT("series value is not finite"))};
        }
        if constexpr (std::is_integral_v<Value>) {
            auto const above_maximum{
                std::is_same_v<Value, uint64>
                    ? value_number >= 18446744073709551616.0
                    : value_number > static_cast<double>(std::numeric_limits<Value>::max())};
            if (FMath::TruncToDouble(value_number) != value_number ||
                value_number < static_cast<double>(std::numeric_limits<Value>::lowest()) ||
                above_maximum) {
                return std::unexpected{
                    error_at(path, TEXT("series value is outside its integer range"))};
            }
        }
        output.add(tick, static_cast<Value>(value_number));
        previous_tick = tick;
    }
    return {};
}

template <typename Series>
auto validate_nonnegative_series(Series const& series, FString const& path)
    -> std::expected<void, FString> {
    auto const count{series.num()};
    for (int32 index{}; index < count; ++index) {
        if (series.value_at(index) < 0) {
            return std::unexpected{error_at(path, TEXT("series values must be nonnegative"))};
        }
    }
    return {};
}

auto parse_realtime_series(FJsonObject const& parent, ml::TimeSeriesData<uint64>& output)
    -> std::expected<void, FString> {
    auto const path{FString{TEXT("completed_ticks_by_real_time")}};
    auto const object{required_object(parent, TEXT("completed_ticks_by_real_time"), path)};
    if (!object) {
        return std::unexpected{object.error()};
    }
    TArray<TSharedPtr<FJsonValue>> const* times{};
    TArray<TSharedPtr<FJsonValue>> const* ticks{};
    if (!(*object)->TryGetArrayField(TEXT("real_elapsed_seconds"), times) ||
        !(*object)->TryGetArrayField(TEXT("completed_ticks"), ticks) || !times || !ticks) {
        return std::unexpected{
            error_at(path, TEXT("real-time and completed-tick arrays are required"))};
    }
    if (times->Num() != ticks->Num()) {
        return std::unexpected{
            error_at(path, TEXT("real-time and completed-tick arrays are not aligned"))};
    }
    output.reserve(times->Num());
    double previous_time{};
    uint64 previous_tick{};
    for (int32 index{}; index < times->Num(); ++index) {
        double time{};
        double tick_number{};
        if (!(*times)[index].IsValid() || !(*times)[index]->TryGetNumber(time) ||
            !FMath::IsFinite(time) || time < 0.0 || (index > 0 && time <= previous_time)) {
            return std::unexpected{error_at(
                path, TEXT("real-time coordinates must be finite, nonnegative, and increasing"))};
        }
        if (!(*ticks)[index].IsValid() || !(*ticks)[index]->TryGetNumber(tick_number) ||
            !FMath::IsFinite(tick_number) || tick_number < 0.0 ||
            FMath::TruncToDouble(tick_number) != tick_number ||
            tick_number >= 18446744073709551616.0) {
            return std::unexpected{error_at(path, TEXT("completed tick is not a valid uint64"))};
        }
        auto const tick{static_cast<uint64>(tick_number)};
        if (index > 0 && tick < previous_tick) {
            return std::unexpected{error_at(path, TEXT("completed ticks must be nondecreasing"))};
        }
        output.add(time, tick);
        previous_time = time;
        previous_tick = tick;
    }
    return {};
}

template <typename Enum>
auto parse_serialized_enum(FString const& value, FString const& path)
    -> std::expected<Enum, FString> {
    Enum result{};
    if (ml::try_parse_serialized(FStringView{value}, result)) {
        return result;
    }
    return std::unexpected{
        error_at(path, FString::Printf(TEXT("unknown enum value '%s'"), *value))};
}
}

auto level_telemetry_runs_directory() -> FString {
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Telemetry"), TEXT("Runs"));
}

auto read_level_telemetry_run(FString const& path)
    -> std::expected<FLevelTelemetryRunRecord, FString> {
    FString json;
    if (!FFileHelper::LoadFileToString(json, *path)) {
        return std::unexpected{FString::Printf(TEXT("Could not read telemetry file '%s'"), *path)};
    }
    auto result{deserialize_level_telemetry_run(json)};
    if (!result) {
        return std::unexpected{FString::Printf(TEXT("%s: %s"), *path, *result.error())};
    }
    return result;
}

auto deserialize_level_telemetry_run(FString const& json)
    -> std::expected<FLevelTelemetryRunRecord, FString> {
    TSharedPtr<FJsonObject> root;
    auto reader{TJsonReaderFactory<>::Create(json)};
    if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid()) {
        return std::unexpected{FString{TEXT("Malformed JSON document")}};
    }

    auto const schema{
        required_integer<int32>(*root, TEXT("schema_version"), TEXT("schema_version"))};
    if (!schema) {
        return std::unexpected{schema.error()};
    }
    if (*schema != 1 && *schema != FLevelTelemetryRunRecord::schema_version) {
        return std::unexpected{
            FString::Printf(TEXT("Unsupported telemetry schema version %d"), *schema)};
    }

    FLevelTelemetryRunRecord result;
    result.loaded_schema_version = *schema;
#define READ_REQUIRED(destination, expression)            \
    do {                                                  \
        auto parsed_value{expression};                    \
        if (!parsed_value) {                              \
            return std::unexpected{parsed_value.error()}; \
        }                                                 \
        destination = MoveTemp(*parsed_value);            \
    } while (false)

    READ_REQUIRED(result.metadata.run_id, required_string(*root, TEXT("run_id"), TEXT("run_id")));
    auto const level{required_object(*root, TEXT("level"), TEXT("level"))};
    auto const timestamps{required_object(*root, TEXT("timestamps"), TEXT("timestamps"))};
    auto const environment{required_object(*root, TEXT("environment"), TEXT("environment"))};
    auto const simulation{required_object(*root, TEXT("simulation"), TEXT("simulation"))};
    auto const completion{required_object(*root, TEXT("completion"), TEXT("completion"))};
    auto const tick_series{required_object(*root, TEXT("tick_series"), TEXT("tick_series"))};
    if (!level || !timestamps || !environment || !simulation || !completion || !tick_series) {
        return std::unexpected{!level         ? level.error()
                               : !timestamps  ? timestamps.error()
                               : !environment ? environment.error()
                               : !simulation  ? simulation.error()
                               : !completion  ? completion.error()
                                              : tick_series.error()};
    }

    READ_REQUIRED(result.metadata.map_name,
                  required_string(**level, TEXT("map_name"), TEXT("level.map_name")));
    FString level_id;
    READ_REQUIRED(level_id, required_string(**level, TEXT("level_id"), TEXT("level.level_id")));
    result.metadata.level_id = FName{level_id};
    READ_REQUIRED(result.metadata.level_display_name,
                  required_string(**level, TEXT("display_name"), TEXT("level.display_name")));
    READ_REQUIRED(
        result.metadata.launched_utc,
        required_string(**timestamps, TEXT("launched_utc"), TEXT("timestamps.launched_utc")));
    READ_REQUIRED(
        result.completion.completed_utc,
        required_string(**timestamps, TEXT("completed_utc"), TEXT("timestamps.completed_utc")));

#define READ_ENV(field)                              \
    READ_REQUIRED(result.metadata.environment.field, \
                  required_string(**environment, TEXT(#field), TEXT("environment." #field)))
    READ_ENV(project_name);
    READ_ENV(project_version);
    READ_ENV(engine_version);
    READ_ENV(build_version);
    READ_ENV(build_configuration);
    READ_ENV(execution_mode);
    READ_ENV(world_type);
    READ_ENV(platform);
    READ_ENV(host_architecture);
    READ_ENV(operating_system_version);
    READ_ENV(operating_system_subversion);
    READ_ENV(cpu_vendor);
    READ_ENV(cpu_brand);
    READ_ENV(primary_gpu_brand);
#undef READ_ENV
    READ_REQUIRED(result.metadata.environment.physical_core_count,
                  required_integer<int32>(**environment,
                                          TEXT("physical_core_count"),
                                          TEXT("environment.physical_core_count")));
    READ_REQUIRED(result.metadata.environment.logical_core_count,
                  required_integer<int32>(**environment,
                                          TEXT("logical_core_count"),
                                          TEXT("environment.logical_core_count")));
    READ_REQUIRED(result.metadata.environment.total_physical_memory_bytes,
                  required_integer<uint64>(**environment,
                                           TEXT("total_physical_memory_bytes"),
                                           TEXT("environment.total_physical_memory_bytes")));
    if (result.metadata.environment.physical_core_count < 0 ||
        result.metadata.environment.logical_core_count < 0) {
        return std::unexpected{FString{TEXT("environment core counts must be nonnegative")}};
    }

    READ_REQUIRED(
        result.metadata.tick_rate_hz,
        required_number(**simulation, TEXT("tick_rate_hz"), TEXT("simulation.tick_rate_hz")));
    READ_REQUIRED(result.metadata.tick_period_seconds,
                  required_number(**simulation,
                                  TEXT("tick_period_seconds"),
                                  TEXT("simulation.tick_period_seconds")));
    READ_REQUIRED(result.metadata.initial_requested_time_scale,
                  required_number(**simulation,
                                  TEXT("initial_requested_time_scale"),
                                  TEXT("simulation.initial_requested_time_scale")));
    READ_REQUIRED(result.metadata.presentation_enabled,
                  required_bool(**simulation,
                                TEXT("presentation_enabled"),
                                TEXT("simulation.presentation_enabled")));
    if (*schema >= 2) {
        READ_REQUIRED(
            result.metadata.source_sha256,
            required_string(**simulation, TEXT("source_sha256"), TEXT("simulation.source_sha256")));
        READ_REQUIRED(
            result.metadata.launch_state,
            required_string(**simulation, TEXT("launch_state"), TEXT("simulation.launch_state")));
        READ_REQUIRED(result.metadata.presentation_mode,
                      required_string(**simulation,
                                      TEXT("presentation_mode"),
                                      TEXT("simulation.presentation_mode")));
        READ_REQUIRED(result.metadata.stop_when_battle_resolved,
                      required_bool(**simulation,
                                    TEXT("stop_when_battle_resolved"),
                                    TEXT("simulation.stop_when_battle_resolved")));
        READ_REQUIRED(result.metadata.results_navigation,
                      required_string(**simulation,
                                      TEXT("results_navigation"),
                                      TEXT("simulation.results_navigation")));
        READ_REQUIRED(result.metadata.battle_sample_interval_seconds,
                      required_number(**simulation,
                                      TEXT("battle_sample_interval_seconds"),
                                      TEXT("simulation.battle_sample_interval_seconds")));
        READ_REQUIRED(result.metadata.performance_window_seconds,
                      required_number(**simulation,
                                      TEXT("performance_window_seconds"),
                                      TEXT("simulation.performance_window_seconds")));
        READ_REQUIRED(result.metadata.detailed_timing_tick_stride,
                      required_integer<uint32>(**simulation,
                                               TEXT("detailed_timing_tick_stride"),
                                               TEXT("simulation.detailed_timing_tick_stride")));
        READ_REQUIRED(result.metadata.detailed_timing,
                      required_bool(**simulation,
                                    TEXT("detailed_timing"),
                                    TEXT("simulation.detailed_timing")));
        auto const* duration{(*simulation)->Values.Find(TEXT("requested_duration_seconds"))};
        if (duration && (*duration)->Type != EJson::Null) {
            double value{};
            READ_REQUIRED(value,
                          required_number(**simulation,
                                          TEXT("requested_duration_seconds"),
                                          TEXT("simulation.requested_duration_seconds")));
            result.metadata.requested_duration_seconds = value;
        }
    }
    if (result.metadata.tick_rate_hz <= 0.0 || result.metadata.tick_period_seconds <= 0.0 ||
        result.metadata.initial_requested_time_scale < 0.0) {
        return std::unexpected{
            FString{TEXT("simulation rates and period are outside their valid ranges")}};
    }

    FString reason_name;
    READ_REQUIRED(reason_name,
                  required_string(**completion, TEXT("reason"), TEXT("completion.reason")));
    READ_REQUIRED(
        result.completion.reason,
        parse_serialized_enum<ELevelTelemetryRunEndReason>(reason_name, TEXT("completion.reason")));
    READ_REQUIRED(result.completion.interrupted,
                  required_bool(**completion, TEXT("interrupted"), TEXT("completion.interrupted")));
    READ_REQUIRED(result.completion.world_end_reason,
                  required_string(
                      **completion, TEXT("world_end_reason"), TEXT("completion.world_end_reason")));
    READ_REQUIRED(result.completion.completed_ticks,
                  required_integer<uint64>(
                      **completion, TEXT("completed_ticks"), TEXT("completion.completed_ticks")));
    READ_REQUIRED(result.completion.simulated_elapsed_seconds,
                  required_number(**completion,
                                  TEXT("simulated_elapsed_seconds"),
                                  TEXT("completion.simulated_elapsed_seconds")));
    READ_REQUIRED(result.completion.wall_elapsed_seconds,
                  required_number(**completion,
                                  TEXT("wall_elapsed_seconds"),
                                  TEXT("completion.wall_elapsed_seconds")));
    if (result.completion.simulated_elapsed_seconds < 0.0 ||
        result.completion.wall_elapsed_seconds < 0.0) {
        return std::unexpected{FString{TEXT("completion durations must be nonnegative")}};
    }
    if (*schema >= 2) {
        auto const* winner{(*completion)->Values.Find(TEXT("winning_team"))};
        if (winner && (*winner)->Type != EJson::Null) {
            FString winner_name;
            READ_REQUIRED(winner_name,
                          required_string(
                              **completion, TEXT("winning_team"), TEXT("completion.winning_team")));
            ETestTeam team{};
            READ_REQUIRED(
                team,
                parse_serialized_enum<ETestTeam>(winner_name, TEXT("completion.winning_team")));
            result.completion.winning_team = team;
        }
    }

    auto const* mission_mode_value{(*completion)->Values.Find(TEXT("mission_mode"))};
    auto const* mission_state_value{(*completion)->Values.Find(TEXT("mission_state"))};
    auto const* mission_fail_value{(*completion)->Values.Find(TEXT("mission_fail_reason"))};
    auto const has_mission_mode{mission_mode_value && (*mission_mode_value)->Type != EJson::Null};
    auto const has_mission_state{mission_state_value &&
                                 (*mission_state_value)->Type != EJson::Null};
    auto const has_mission_fail{mission_fail_value && (*mission_fail_value)->Type != EJson::Null};
    if (has_mission_mode != has_mission_state || has_mission_mode != has_mission_fail) {
        return std::unexpected{FString{
            TEXT("completion mission mode, state, and fail reason must be provided together")}};
    }
    if (has_mission_mode) {
        FString mode_name;
        FString state_name;
        FString fail_name;
        READ_REQUIRED(
            mode_name,
            required_string(**completion, TEXT("mission_mode"), TEXT("completion.mission_mode")));
        READ_REQUIRED(
            state_name,
            required_string(**completion, TEXT("mission_state"), TEXT("completion.mission_state")));
        READ_REQUIRED(fail_name,
                      required_string(**completion,
                                      TEXT("mission_fail_reason"),
                                      TEXT("completion.mission_fail_reason")));
        ETestMissionMode mode{};
        ETestMissionState state{};
        ETestMissionFailReason fail{};
        READ_REQUIRED(
            mode,
            parse_serialized_enum<ETestMissionMode>(mode_name, TEXT("completion.mission_mode")));
        READ_REQUIRED(
            state,
            parse_serialized_enum<ETestMissionState>(state_name, TEXT("completion.mission_state")));
        READ_REQUIRED(fail,
                      parse_serialized_enum<ETestMissionFailReason>(
                          fail_name, TEXT("completion.mission_fail_reason")));
        result.completion.mission_mode = mode;
        result.completion.mission_state = state;
        result.completion.mission_fail_reason = fail;
    }
    auto const* mission_elapsed{(*completion)->Values.Find(TEXT("mission_elapsed_seconds"))};
    if (mission_elapsed && (*mission_elapsed)->Type != EJson::Null) {
        double elapsed{};
        READ_REQUIRED(elapsed,
                      required_number(**completion,
                                      TEXT("mission_elapsed_seconds"),
                                      TEXT("completion.mission_elapsed_seconds")));
        if (elapsed < 0.0) {
            return std::unexpected{
                FString{TEXT("completion.mission_elapsed_seconds must be nonnegative")}};
        }
        result.completion.mission_elapsed_seconds = elapsed;
    }

#define PARSE_SERIES(field)                                                                       \
    do {                                                                                          \
        auto parsed{parse_tick_series(                                                            \
            **tick_series, TEXT(#field), TEXT("tick_series." #field), result.tick_series.field)}; \
        if (!parsed) {                                                                            \
            return std::unexpected{parsed.error()};                                               \
        }                                                                                         \
    } while (false)
    PARSE_SERIES(active_entities);
    PARSE_SERIES(spawned_entities);
    PARSE_SERIES(destroyed_entities);
    PARSE_SERIES(kills);
    PARSE_SERIES(registry_slot_count);
    PARSE_SERIES(active_lasers);
    PARSE_SERIES(lasers_fired);
    PARSE_SERIES(occupied_spatial_cell_count);
    PARSE_SERIES(grid_rebuild_count);
    PARSE_SERIES(range_query_count);
    PARSE_SERIES(line_trace_count);
    PARSE_SERIES(sweep_trace_count);
    PARSE_SERIES(requested_time_scale);
#undef PARSE_SERIES
#define VALIDATE_COUNT(field)                                                                    \
    do {                                                                                         \
        auto valid{                                                                              \
            validate_nonnegative_series(result.tick_series.field, TEXT("tick_series." #field))}; \
        if (!valid) {                                                                            \
            return std::unexpected{valid.error()};                                               \
        }                                                                                        \
    } while (false)
    VALIDATE_COUNT(active_entities);
    VALIDATE_COUNT(spawned_entities);
    VALIDATE_COUNT(destroyed_entities);
    VALIDATE_COUNT(kills);
    VALIDATE_COUNT(registry_slot_count);
    VALIDATE_COUNT(active_lasers);
    VALIDATE_COUNT(lasers_fired);
    VALIDATE_COUNT(occupied_spatial_cell_count);
#undef VALIDATE_COUNT
    auto requested_valid{validate_nonnegative_series(result.tick_series.requested_time_scale,
                                                     TEXT("tick_series.requested_time_scale"))};
    if (!requested_valid) {
        return std::unexpected{requested_valid.error()};
    }
    auto const by_type{required_object(**tick_series,
                                       TEXT("active_entities_by_type"),
                                       TEXT("tick_series.active_entities_by_type"))};
    auto const by_team{required_object(**tick_series,
                                       TEXT("active_entities_by_team_and_type"),
                                       TEXT("tick_series.active_entities_by_team_and_type"))};
    if (!by_type || !by_team) {
        return std::unexpected{!by_type ? by_type.error() : by_team.error()};
    }
    for (int32 type{}; type < FLevelTelemetryTickSeries::entity_type_count; ++type) {
        auto const* type_name{LexToSerializedString(static_cast<ETestEntityType>(type))};
        auto parsed{parse_tick_series(
            **by_type,
            type_name,
            FString::Printf(TEXT("tick_series.active_entities_by_type.%s"), type_name),
            result.tick_series.active_entities_by_type[type])};
        if (!parsed) {
            return std::unexpected{parsed.error()};
        }
        auto valid{validate_nonnegative_series(
            result.tick_series.active_entities_by_type[type],
            FString::Printf(TEXT("tick_series.active_entities_by_type.%s"), type_name))};
        if (!valid) {
            return std::unexpected{valid.error()};
        }
    }
    for (int32 team{}; team < FLevelTelemetryTickSeries::team_count; ++team) {
        auto const* team_name{LexToSerializedString(static_cast<ETestTeam>(team))};
        auto const team_object{required_object(
            **by_team,
            team_name,
            FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s"), team_name))};
        if (!team_object) {
            return std::unexpected{team_object.error()};
        }
        for (int32 type{}; type < FLevelTelemetryTickSeries::entity_type_count; ++type) {
            auto const* type_name{LexToSerializedString(static_cast<ETestEntityType>(type))};
            auto parsed{parse_tick_series(
                **team_object,
                type_name,
                FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s.%s"),
                                team_name,
                                type_name),
                result.tick_series.active_entities_by_team_and_type[team][type])};
            if (!parsed) {
                return std::unexpected{parsed.error()};
            }
            auto valid{validate_nonnegative_series(
                result.tick_series.active_entities_by_team_and_type[team][type],
                FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s.%s"),
                                team_name,
                                type_name))};
            if (!valid) {
                return std::unexpected{valid.error()};
            }
        }
    }
    auto realtime{parse_realtime_series(*root, result.completed_ticks_by_real_time)};
    if (!realtime) {
        return std::unexpected{realtime.error()};
    }
    if (*schema >= 2) {
        TArray<TSharedPtr<FJsonValue>> const* battle_samples{};
        if (!root->TryGetArrayField(TEXT("battle_samples"), battle_samples) || !battle_samples) {
            return std::unexpected{FString{TEXT("battle_samples: required array is missing")}};
        }
        result.battle_samples.Reserve(battle_samples->Num());
        for (int32 index{}; index < battle_samples->Num(); ++index) {
            auto const object{(*battle_samples)[index]->AsObject()};
            auto const path{FString::Printf(TEXT("battle_samples[%d]"), index)};
            if (!object.IsValid()) {
                return std::unexpected{error_at(path, TEXT("sample must be an object"))};
            }
            FLevelTelemetryBattleSample sample;
            READ_REQUIRED(sample.completed_tick,
                          required_integer<uint64>(
                              *object, TEXT("completed_tick"), path + TEXT(".completed_tick")));
            READ_REQUIRED(sample.simulated_elapsed_seconds,
                          required_number(*object,
                                          TEXT("simulated_elapsed_seconds"),
                                          path + TEXT(".simulated_elapsed_seconds")));
            auto alive{
                parse_flat_counts(*object, TEXT("alive"), path + TEXT(".alive"), sample.alive)};
            auto combat{required_object(*object, TEXT("combat"), path + TEXT(".combat"))};
            if (!alive || !combat) {
                return std::unexpected{!alive ? alive.error() : combat.error()};
            }
            auto parsed_combat{parse_combat(**combat, sample.combat, path + TEXT(".combat"))};
            if (!parsed_combat) {
                return std::unexpected{parsed_combat.error()};
            }
            double optional_number{};
#define PARSE_OPTIONAL_BATTLE_NUMBER(field)                                            \
    if (object->TryGetNumberField(TEXT(#field), optional_number)) {                    \
        if (!FMath::IsFinite(optional_number) || optional_number < 0.0) {              \
            return std::unexpected{                                                    \
                error_at(path + TEXT("." #field), TEXT("value must be nonnegative"))}; \
        }                                                                              \
        sample.field = static_cast<decltype(sample.field)>(optional_number);           \
    }
            PARSE_OPTIONAL_BATTLE_NUMBER(active_lasers);
            PARSE_OPTIONAL_BATTLE_NUMBER(lasers_fired);
            PARSE_OPTIONAL_BATTLE_NUMBER(registry_slot_count);
            PARSE_OPTIONAL_BATTLE_NUMBER(occupied_spatial_cell_count);
            PARSE_OPTIONAL_BATTLE_NUMBER(grid_rebuild_count);
            PARSE_OPTIONAL_BATTLE_NUMBER(range_query_count);
            PARSE_OPTIONAL_BATTLE_NUMBER(line_trace_count);
            PARSE_OPTIONAL_BATTLE_NUMBER(sweep_trace_count);
#undef PARSE_OPTIONAL_BATTLE_NUMBER
            result.battle_samples.Add(MoveTemp(sample));
        }

        TArray<TSharedPtr<FJsonValue>> const* windows{};
        if (!root->TryGetArrayField(TEXT("performance_windows"), windows) || !windows) {
            return std::unexpected{FString{TEXT("performance_windows: required array is missing")}};
        }
        result.performance_windows.Reserve(windows->Num());
        for (int32 index{}; index < windows->Num(); ++index) {
            auto const object{(*windows)[index]->AsObject()};
            auto const path{FString::Printf(TEXT("performance_windows[%d]"), index)};
            if (!object.IsValid()) {
                return std::unexpected{error_at(path, TEXT("window must be an object"))};
            }
            FLevelTelemetryPerformanceWindow window;
            READ_REQUIRED(window.real_elapsed_seconds,
                          required_number(*object,
                                          TEXT("real_elapsed_seconds"),
                                          path + TEXT(".real_elapsed_seconds")));
            READ_REQUIRED(window.completed_tick,
                          required_integer<uint64>(
                              *object, TEXT("completed_tick"), path + TEXT(".completed_tick")));
#define PARSE_TIMING(field)                                                                      \
    do {                                                                                         \
        auto parsed{parse_timing(*object, TEXT(#field), path + TEXT("." #field), window.field)}; \
        if (!parsed) {                                                                           \
            return std::unexpected{parsed.error()};                                              \
        }                                                                                        \
    } while (false)
            PARSE_TIMING(frame);
            PARSE_TIMING(game_thread);
            PARSE_TIMING(render_thread);
            PARSE_TIMING(gpu);
            PARSE_TIMING(simulation_tick);
#undef PARSE_TIMING
            TArray<TSharedPtr<FJsonValue>> const* systems{};
            if (!object->TryGetArrayField(TEXT("systems"), systems) || !systems ||
                systems->Num() != FLevelTelemetryPerformanceWindow::system_count) {
                return std::unexpected{
                    error_at(path + TEXT(".systems"), TEXT("timing array is missing or invalid"))};
            }
            for (int32 system{}; system < systems->Num(); ++system) {
                auto const timing_object{(*systems)[system]->AsObject()};
                if (!timing_object.IsValid()) {
                    return std::unexpected{
                        error_at(path + TEXT(".systems"), TEXT("timing must be an object"))};
                }
                auto wrapper{MakeShared<FJsonObject>()};
                wrapper->SetObjectField(TEXT("value"), timing_object);
                auto parsed{parse_timing(
                    *wrapper, TEXT("value"), path + TEXT(".systems"), window.systems[system])};
                if (!parsed) {
                    return std::unexpected{parsed.error()};
                }
            }
            TArray<TSharedPtr<FJsonValue>> const* phases{};
            TArray<TSharedPtr<FJsonValue>> const* phase_cpu_share{};
            if (!object->TryGetArrayField(TEXT("phases"), phases) || !phases ||
                phases->Num() != FLevelTelemetryPerformanceWindow::phase_count ||
                !object->TryGetArrayField(TEXT("phase_cpu_share"), phase_cpu_share) ||
                !phase_cpu_share ||
                phase_cpu_share->Num() != FLevelTelemetryPerformanceWindow::phase_count) {
                return std::unexpected{
                    error_at(path + TEXT(".phases"), TEXT("phase arrays are missing or invalid"))};
            }
            for (int32 phase{}; phase < phases->Num(); ++phase) {
                auto const timing_object{(*phases)[phase]->AsObject()};
                if (!timing_object.IsValid()) {
                    return std::unexpected{
                        error_at(path + TEXT(".phases"), TEXT("timing must be an object"))};
                }
                auto wrapper{MakeShared<FJsonObject>()};
                wrapper->SetObjectField(TEXT("value"), timing_object);
                auto parsed{parse_timing(
                    *wrapper, TEXT("value"), path + TEXT(".phases"), window.phases[phase])};
                if (!parsed) {
                    return std::unexpected{parsed.error()};
                }
                auto const share{(*phase_cpu_share)[phase]->AsNumber()};
                if (!FMath::IsFinite(share) || share < 0.0) {
                    return std::unexpected{error_at(path + TEXT(".phase_cpu_share"),
                                                    TEXT("shares must be nonnegative"))};
                }
                window.phase_cpu_share[phase] = share;
            }
            result.performance_windows.Add(MoveTemp(window));
        }
    }
#undef READ_REQUIRED
    return result;
}

auto serialize_level_telemetry_run(FLevelTelemetryRunRecord const& record) -> FString {
    auto root{MakeShared<FJsonObject>()};
    root->SetNumberField(TEXT("schema_version"), FLevelTelemetryRunRecord::schema_version);
    root->SetStringField(TEXT("run_id"), record.metadata.run_id);

    auto level{MakeShared<FJsonObject>()};
    level->SetStringField(TEXT("map_name"), record.metadata.map_name);
    level->SetStringField(TEXT("level_id"), record.metadata.level_id.ToString());
    level->SetStringField(TEXT("display_name"), record.metadata.level_display_name);
    root->SetObjectField(TEXT("level"), level);

    auto timestamps{MakeShared<FJsonObject>()};
    timestamps->SetStringField(TEXT("launched_utc"), record.metadata.launched_utc);
    timestamps->SetStringField(TEXT("completed_utc"), record.completion.completed_utc);
    root->SetObjectField(TEXT("timestamps"), timestamps);

    auto environment{MakeShared<FJsonObject>()};
    auto const& source{record.metadata.environment};
    environment->SetStringField(TEXT("project_name"), source.project_name);
    environment->SetStringField(TEXT("project_version"), source.project_version);
    environment->SetStringField(TEXT("engine_version"), source.engine_version);
    environment->SetStringField(TEXT("build_version"), source.build_version);
    environment->SetStringField(TEXT("build_configuration"), source.build_configuration);
    environment->SetStringField(TEXT("execution_mode"), source.execution_mode);
    environment->SetStringField(TEXT("world_type"), source.world_type);
    environment->SetStringField(TEXT("platform"), source.platform);
    environment->SetStringField(TEXT("host_architecture"), source.host_architecture);
    environment->SetStringField(TEXT("operating_system_version"), source.operating_system_version);
    environment->SetStringField(TEXT("operating_system_subversion"),
                                source.operating_system_subversion);
    environment->SetStringField(TEXT("cpu_vendor"), source.cpu_vendor);
    environment->SetStringField(TEXT("cpu_brand"), source.cpu_brand);
    environment->SetNumberField(TEXT("physical_core_count"), source.physical_core_count);
    environment->SetNumberField(TEXT("logical_core_count"), source.logical_core_count);
    environment->SetStringField(TEXT("primary_gpu_brand"), source.primary_gpu_brand);
    environment->SetNumberField(TEXT("total_physical_memory_bytes"),
                                static_cast<double>(source.total_physical_memory_bytes));
    root->SetObjectField(TEXT("environment"), environment);

    auto simulation{MakeShared<FJsonObject>()};
    simulation->SetNumberField(TEXT("tick_rate_hz"), record.metadata.tick_rate_hz);
    simulation->SetNumberField(TEXT("tick_period_seconds"), record.metadata.tick_period_seconds);
    simulation->SetNumberField(TEXT("initial_requested_time_scale"),
                               record.metadata.initial_requested_time_scale);
    simulation->SetBoolField(TEXT("presentation_enabled"), record.metadata.presentation_enabled);
    simulation->SetStringField(TEXT("source_sha256"), record.metadata.source_sha256);
    simulation->SetStringField(TEXT("launch_state"), record.metadata.launch_state);
    simulation->SetStringField(TEXT("presentation_mode"), record.metadata.presentation_mode);
    simulation->SetBoolField(TEXT("stop_when_battle_resolved"),
                             record.metadata.stop_when_battle_resolved);
    simulation->SetStringField(TEXT("results_navigation"), record.metadata.results_navigation);
    set_optional_number(*simulation,
                        TEXT("requested_duration_seconds"),
                        record.metadata.requested_duration_seconds);
    simulation->SetNumberField(TEXT("battle_sample_interval_seconds"),
                               record.metadata.battle_sample_interval_seconds);
    simulation->SetNumberField(TEXT("performance_window_seconds"),
                               record.metadata.performance_window_seconds);
    simulation->SetNumberField(TEXT("detailed_timing_tick_stride"),
                               record.metadata.detailed_timing_tick_stride);
    simulation->SetBoolField(TEXT("detailed_timing"), record.metadata.detailed_timing);
    root->SetObjectField(TEXT("simulation"), simulation);

    auto completion{MakeShared<FJsonObject>()};
    completion->SetStringField(TEXT("reason"), LexToSerializedString(record.completion.reason));
    completion->SetBoolField(TEXT("interrupted"), record.completion.interrupted);
    completion->SetStringField(TEXT("world_end_reason"), record.completion.world_end_reason);
    if (record.completion.mission_mode.IsSet()) {
        completion->SetStringField(
            TEXT("mission_mode"), LexToSerializedString(record.completion.mission_mode.GetValue()));
        completion->SetStringField(
            TEXT("mission_state"),
            LexToSerializedString(record.completion.mission_state.GetValue()));
        completion->SetStringField(
            TEXT("mission_fail_reason"),
            LexToSerializedString(record.completion.mission_fail_reason.GetValue()));
    } else {
        completion->SetField(TEXT("mission_mode"), MakeShared<FJsonValueNull>());
        completion->SetField(TEXT("mission_state"), MakeShared<FJsonValueNull>());
        completion->SetField(TEXT("mission_fail_reason"), MakeShared<FJsonValueNull>());
    }
    set_optional_number(
        *completion, TEXT("mission_elapsed_seconds"), record.completion.mission_elapsed_seconds);
    completion->SetNumberField(TEXT("completed_ticks"),
                               static_cast<double>(record.completion.completed_ticks));
    completion->SetNumberField(TEXT("simulated_elapsed_seconds"),
                               record.completion.simulated_elapsed_seconds);
    completion->SetNumberField(TEXT("wall_elapsed_seconds"),
                               record.completion.wall_elapsed_seconds);
    if (record.completion.winning_team.IsSet()) {
        completion->SetStringField(
            TEXT("winning_team"), LexToSerializedString(record.completion.winning_team.GetValue()));
    } else {
        completion->SetField(TEXT("winning_team"), MakeShared<FJsonValueNull>());
    }
    root->SetObjectField(TEXT("completion"), completion);
    root->SetObjectField(TEXT("completed_ticks_by_real_time"),
                         make_realtime_series(record.completed_ticks_by_real_time));
    root->SetObjectField(TEXT("tick_series"), make_tick_series(record.tick_series));
    root->SetArrayField(TEXT("battle_samples"), make_battle_samples(record.battle_samples));
    root->SetArrayField(TEXT("performance_windows"),
                        make_performance_windows(record.performance_windows));

    FString output;
    auto writer{TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&output)};
    if (!FJsonSerializer::Serialize(root, writer)) {
        return {};
    }
    return output;
}

auto write_level_telemetry_run(FLevelTelemetryRunRecord const& record,
                               FString const& output_directory) -> std::expected<FString, FString> {
    auto const json{serialize_level_telemetry_run(record)};
    if (json.IsEmpty()) {
        return std::unexpected{FString{TEXT("JSON serialization failed")}};
    }
    if (!IFileManager::Get().MakeDirectory(*output_directory, true)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not create output directory '%s'"), *output_directory)};
    }

    auto timestamp{record.metadata.launched_utc};
    timestamp.ReplaceCharInline(TEXT(':'), TEXT('-'));
    auto level_name{record.metadata.level_id.IsNone() ? record.metadata.map_name
                                                      : record.metadata.level_id.ToString()};
    level_name = FPaths::MakeValidFileName(level_name);
    auto const short_run_id{record.metadata.run_id.Left(8)};
    auto const filename{
        FString::Printf(TEXT("%s_%s_%s.json"), *timestamp, *level_name, *short_run_id)};
    auto const path{FPaths::Combine(output_directory, filename)};
    auto const temporary_path{path + TEXT(".tmp")};
    if (!FFileHelper::SaveStringToFile(
            json, *temporary_path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not write temporary file '%s'"), *temporary_path)};
    }
    if (!IFileManager::Get().Move(*path, *temporary_path)) {
        IFileManager::Get().Delete(*temporary_path, false, true);
        return std::unexpected{FString::Printf(TEXT("Could not replace output file '%s'"), *path)};
    }
    return path;
}
