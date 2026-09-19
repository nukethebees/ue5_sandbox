#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/missions/NativeMissionTypes.h>
#include <SpaceGameSimulation/telemetry/LevelTelemetryRunEndReason.h>

#include <ioj/sim/telemetry/level_telemetry_json_validation.h>

#include <SandboxCore/container_ops.h>

#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

#include <string_view>
#include <type_traits>

namespace {

auto to_unreal_string(std::string_view const value) -> FString {
    return UTF8_TO_TCHAR(value.data());
}

auto serialized_name(::ioj::sim::EntityType const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

auto serialized_name(::ioj::sim::Team const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

auto serialized_name(::ioj::sim::MissionMode const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

auto serialized_name(::ioj::sim::MissionState const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

auto serialized_name(::ioj::sim::MissionFailReason const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

auto serialized_name(::ioj::sim::LevelTelemetryRunEndReason const value) -> FString {
    return to_unreal_string(::ioj::sim::to_serialized_string(value));
}

void set_optional_number(FJsonObject& object,
                         FString const& name,
                         std::optional<double> const value) {
    if (value.has_value()) {
        object.SetNumberField(name, value.value());
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

auto make_tick_series(::ioj::sim::LevelTelemetryTickSeries const& source)
    -> TSharedRef<FJsonObject> {
    auto result{MakeShared<FJsonObject>()};
    result->SetObjectField(TEXT("active_entities"), make_series(source.active_entities));

    auto active_by_type{MakeShared<FJsonObject>()};
    constexpr auto entity_type_count{::ioj::sim::LevelTelemetryTickSeries::entity_type_count};
    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        active_by_type->SetObjectField(
            serialized_name(static_cast<::ioj::sim::EntityType>(entity_type_index)),
            make_series(source.active_entities_by_type[entity_type_index]));
    }
    result->SetObjectField(TEXT("active_entities_by_type"), active_by_type);

    auto active_by_team_and_type{MakeShared<FJsonObject>()};
    constexpr auto team_count{::ioj::sim::LevelTelemetryTickSeries::team_count};
    for (int32 team_index{}; team_index < team_count; ++team_index) {
        auto team{MakeShared<FJsonObject>()};
        for (int32 entity_type_index{}; entity_type_index < entity_type_count;
             ++entity_type_index) {
            team->SetObjectField(
                serialized_name(static_cast<::ioj::sim::EntityType>(entity_type_index)),
                make_series(
                    source.active_entities_by_team_and_type[team_index][entity_type_index]));
        }
        active_by_team_and_type->SetObjectField(
            serialized_name(ml::to_native(static_cast<ETestTeam>(team_index))), team);
    }
    result->SetObjectField(TEXT("active_entities_by_team_and_type"), active_by_team_and_type);

    result->SetObjectField(TEXT("spawned_entities"), make_series(source.spawned_entities));
    result->SetObjectField(TEXT("destroyed_entities"), make_series(source.destroyed_entities));
    result->SetObjectField(TEXT("kills"), make_series(source.kills));
    result->SetObjectField(TEXT("active_lasers"), make_series(source.active_lasers));
    result->SetObjectField(TEXT("lasers_fired"), make_series(source.lasers_fired));
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

auto make_combat(::ioj::sim::telemetry::CombatTelemetryCounters const& source)
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

auto make_battle_samples(TArray<::ioj::sim::LevelTelemetryBattleSample> const& source)
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

auto required_native_string(FJsonObject const& source, TCHAR const* field, FString const& path)
    -> std::expected<std::string, FString> {
    auto result{required_string(source, field, path)};
    if (!result) {
        return std::unexpected{result.error()};
    }
    return std::string{TCHAR_TO_UTF8(**result)};
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

auto required_number(TSharedPtr<FJsonValue> const& source, FString const& path)
    -> std::expected<double, FString> {
    double result{};
    if (!source.IsValid() || !source->TryGetNumber(result) || !FMath::IsFinite(result)) {
        return std::unexpected{
            error_at(path, TEXT("required finite number is missing or invalid"))};
    }
    return result;
}

template <typename Integer>
auto number_to_integer(double const value, FString const& path) -> std::expected<Integer, FString> {
    auto const native_path{std::string{TCHAR_TO_UTF8(*path)}};
    if constexpr (std::is_same_v<Integer, int32>) {
        auto const parsed{::ioj::sim::telemetry::parse_json_int32(value, native_path)};
        if (parsed) {
            return *parsed;
        }
        return std::unexpected{UTF8_TO_TCHAR(parsed.error().c_str())};
    } else {
        static_assert(std::is_same_v<Integer, uint64>);
        auto const parsed{::ioj::sim::telemetry::parse_json_uint64(value, native_path)};
        if (parsed) {
            return *parsed;
        }
        return std::unexpected{UTF8_TO_TCHAR(parsed.error().c_str())};
    }
}

template <typename Integer>
auto required_integer(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<Integer, FString> {
    auto const number{required_number(source, field, path)};
    if (!number) {
        return std::unexpected{number.error()};
    }
    auto const parsed{number_to_integer<Integer>(*number, path)};
    if (!parsed) {
        return std::unexpected{parsed.error()};
    }
    return *parsed;
}

template <typename Integer>
auto optional_integer(FJsonObject const& source, TCHAR const* const field, FString const& path)
    -> std::expected<std::optional<Integer>, FString> {
    auto const* value{source.Values.Find(field)};
    if (!value) {
        return std::optional<Integer>{};
    }
    auto const number{required_number(*value, path)};
    if (!number) {
        return std::unexpected{number.error()};
    }
    auto const parsed{number_to_integer<Integer>(*number, path)};
    if (!parsed) {
        return std::unexpected{parsed.error()};
    }
    return std::optional<Integer>{*parsed};
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
        expected_count += static_cast<int32>(row.size());
    }
    if (values->Num() != expected_count) {
        return std::unexpected{error_at(path, TEXT("array has the wrong number of values"))};
    }
    int32 index{};
    for (auto& row : output) {
        for (auto& cell : row) {
            auto const value_path{FString::Printf(TEXT("%s[%d]"), *path, index++)};
            auto const value{required_number((*values)[index - 1], value_path)};
            if (!value) {
                return std::unexpected{value.error()};
            }
            if (*value < 0.0) {
                return std::unexpected{error_at(value_path, TEXT("value must be nonnegative"))};
            }
            using Value = std::remove_cvref_t<decltype(cell)>;
            if constexpr (std::is_integral_v<Value>) {
                auto const parsed{number_to_integer<Value>(*value, value_path)};
                if (!parsed) {
                    return std::unexpected{parsed.error()};
                }
                cell = *parsed;
            } else {
                cell = static_cast<Value>(*value);
            }
        }
    }
    return {};
}

auto parse_combat(FJsonObject const& source,
                  ::ioj::sim::telemetry::CombatTelemetryCounters& output,
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
        auto const tick_path{FString::Printf(TEXT("%s.ticks[%d]"), *path, index)};
        auto const tick_number{required_number((*ticks)[index], tick_path)};
        if (!tick_number) {
            return std::unexpected{tick_number.error()};
        }
        auto const tick{number_to_integer<uint64>(*tick_number, tick_path)};
        if (!tick) {
            return std::unexpected{tick.error()};
        }
        if (index > 0 && *tick <= previous_tick) {
            return std::unexpected{
                error_at(tick_path, TEXT("tick coordinates must be strictly increasing"))};
        }
        auto const value_path{FString::Printf(TEXT("%s.values[%d]"), *path, index)};
        auto const value_number{required_number((*values)[index], value_path)};
        if (!value_number) {
            return std::unexpected{value_number.error()};
        }
        if constexpr (std::is_integral_v<Value>) {
            auto const value{number_to_integer<Value>(*value_number, value_path)};
            if (!value) {
                return std::unexpected{value.error()};
            }
            output.add(*tick, *value);
        } else {
            output.add(*tick, static_cast<Value>(*value_number));
        }
        previous_tick = *tick;
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

template <typename Enum>
auto parse_serialized_enum(FString const& value, FString const& path)
    -> std::expected<Enum, FString> {
    auto const utf8_value{FTCHARToUTF8{*value}};
    auto const encoded{
        std::string_view{utf8_value.Get(), static_cast<std::size_t>(utf8_value.Length())}};
    if constexpr (std::is_same_v<Enum, ETestTeam>) {
        if (auto const result{::ioj::sim::try_parse_serialized_team(encoded)}) {
            return ml::to_unreal(*result);
        }
    } else if constexpr (std::is_same_v<Enum, ETestMissionMode>) {
        if (auto const result{::ioj::sim::try_parse_serialized_mission_mode(encoded)}) {
            return ml::to_unreal(*result);
        }
    } else if constexpr (std::is_same_v<Enum, ETestMissionState>) {
        if (auto const result{::ioj::sim::try_parse_serialized_mission_state(encoded)}) {
            return ml::to_unreal(*result);
        }
    } else if constexpr (std::is_same_v<Enum, ETestMissionFailReason>) {
        if (auto const result{::ioj::sim::try_parse_serialized_mission_fail_reason(encoded)}) {
            return ml::to_unreal(*result);
        }
    } else if constexpr (std::is_same_v<Enum, ELevelTelemetryRunEndReason>) {
        if (auto const result{
                ::ioj::sim::try_parse_serialized_level_telemetry_run_end_reason(encoded)}) {
            return *result;
        }
    } else {
        static_assert(!std::is_same_v<Enum, Enum>, "Unsupported serialized enum");
    }
    return std::unexpected{
        error_at(path, FString::Printf(TEXT("unknown enum value '%s'"), *value))};
}

template <typename Integer>
auto validate_serialized_integer(Integer const value, FString const& path, bool const nonnegative)
    -> std::expected<void, FString> {
    static_assert(std::is_integral_v<Integer>);

    if constexpr (std::is_signed_v<Integer>) {
        if (nonnegative && value < 0) {
            return std::unexpected{error_at(path, TEXT("value must be nonnegative"))};
        }
    }
    if constexpr (std::numeric_limits<Integer>::digits > 53) {
        constexpr auto exact_integer_limit{static_cast<Integer>(9007199254740992ULL)};
        if (value > exact_integer_limit) {
            return std::unexpected{
                error_at(path, TEXT("integer cannot be represented exactly in JSON"))};
        }
    }
    return {};
}

auto validate_serialized_number(double const value, FString const& path, bool const nonnegative)
    -> std::expected<void, FString> {
    if (!FMath::IsFinite(value)) {
        return std::unexpected{error_at(path, TEXT("value must be finite"))};
    }
    if (nonnegative && value < 0.0) {
        return std::unexpected{error_at(path, TEXT("value must be nonnegative"))};
    }
    return {};
}

template <typename Counts>
auto validate_serialized_counts(Counts const& source, FString const& path)
    -> std::expected<void, FString> {
    int32 index{};
    for (auto const& row : source) {
        for (auto const value : row) {
            auto const value_path{FString::Printf(TEXT("%s[%d]"), *path, index++)};
            if constexpr (std::is_integral_v<decltype(value)>) {
                auto const valid{validate_serialized_integer(value, value_path, true)};
                if (!valid) {
                    return valid;
                }
            } else {
                auto const valid{validate_serialized_number(value, value_path, true)};
                if (!valid) {
                    return valid;
                }
            }
        }
    }
    return {};
}

template <typename Series>
auto validate_serialized_series(Series const& source, FString const& path)
    -> std::expected<void, FString> {
    auto const count{source.num()};
    uint64 previous_tick{};
    for (int32 index{}; index < count; ++index) {
        auto const tick_path{FString::Printf(TEXT("%s.ticks[%d]"), *path, index)};
        auto const tick{source.time_at(index)};
        auto const valid_tick{validate_serialized_integer(tick, tick_path, true)};
        if (!valid_tick) {
            return valid_tick;
        }
        if (index > 0 && tick <= previous_tick) {
            return std::unexpected{
                error_at(tick_path, TEXT("tick coordinates must be strictly increasing"))};
        }
        auto const valid_value{validate_serialized_integer(
            source.value_at(index), FString::Printf(TEXT("%s.values[%d]"), *path, index), true)};
        if (!valid_value) {
            return valid_value;
        }
        previous_tick = tick;
    }
    return {};
}

auto validate_serialized_report(FLevelTelemetryReport const& record)
    -> std::expected<void, FString> {
    ::ioj::sim::LevelTelemetryRunRecord native_record;
    native_record.metadata = record.metadata;
    native_record.completion = record.completion;
    native_record.tick_series = record.tick_series;
    native_record.battle_samples.assign(record.battle_samples.begin(), record.battle_samples.end());
    auto const native{::ioj::sim::telemetry::validate_level_telemetry_json_record(native_record)};
    if (!native) {
        return std::unexpected{UTF8_TO_TCHAR(native.error().c_str())};
    }

    auto const& environment{record.metadata.environment};
    auto const physical_cores{validate_serialized_integer(
        environment.physical_core_count, TEXT("environment.physical_core_count"), true)};
    auto const logical_cores{validate_serialized_integer(
        environment.logical_core_count, TEXT("environment.logical_core_count"), true)};
    auto const memory{validate_serialized_integer(environment.total_physical_memory_bytes,
                                                  TEXT("environment.total_physical_memory_bytes"),
                                                  true)};
    if (!physical_cores || !logical_cores || !memory) {
        return std::unexpected{!physical_cores  ? physical_cores.error()
                               : !logical_cores ? logical_cores.error()
                                                : memory.error()};
    }

    auto const tick_rate{validate_serialized_number(
        record.metadata.tick_rate_hz, TEXT("simulation.tick_rate_hz"), false)};
    auto const tick_period{validate_serialized_number(
        record.metadata.tick_period_seconds, TEXT("simulation.tick_period_seconds"), false)};
    auto const time_scale{
        validate_serialized_number(record.metadata.initial_requested_time_scale,
                                   TEXT("simulation.initial_requested_time_scale"),
                                   true)};
    auto const sample_interval{
        validate_serialized_number(record.metadata.battle_sample_interval_seconds,
                                   TEXT("simulation.battle_sample_interval_seconds"),
                                   false)};
    if (!tick_rate || !tick_period || !time_scale || !sample_interval) {
        return std::unexpected{!tick_rate     ? tick_rate.error()
                               : !tick_period ? tick_period.error()
                               : !time_scale  ? time_scale.error()
                                              : sample_interval.error()};
    }
    if (record.metadata.tick_rate_hz <= 0.0 || record.metadata.tick_period_seconds <= 0.0) {
        return std::unexpected{
            FString{TEXT("simulation rates and period are outside their valid ranges")}};
    }
    if (record.metadata.requested_duration_seconds.has_value()) {
        auto const duration{
            validate_serialized_number(*record.metadata.requested_duration_seconds,
                                       TEXT("simulation.requested_duration_seconds"),
                                       true)};
        if (!duration) {
            return duration;
        }
    }

    auto const mission_values{record.completion.mission_mode.has_value()};
    if (mission_values != record.completion.mission_state.has_value() ||
        mission_values != record.completion.mission_fail_reason.has_value()) {
        return std::unexpected{FString{
            TEXT("completion mission mode, state, and fail reason must be provided together")}};
    }
    auto const completed_ticks{validate_serialized_integer(
        record.completion.completed_ticks, TEXT("completion.completed_ticks"), true)};
    auto const completion_elapsed{
        validate_serialized_number(record.completion.simulated_elapsed_seconds,
                                   TEXT("completion.simulated_elapsed_seconds"),
                                   true)};
    if (!completed_ticks || !completion_elapsed) {
        return std::unexpected{!completed_ticks ? completed_ticks.error()
                                                : completion_elapsed.error()};
    }
    if (record.completion.mission_elapsed_seconds.has_value()) {
        auto const mission_elapsed{
            validate_serialized_number(*record.completion.mission_elapsed_seconds,
                                       TEXT("completion.mission_elapsed_seconds"),
                                       true)};
        if (!mission_elapsed) {
            return mission_elapsed;
        }
    }

#define VALIDATE_SERIES(field)                                                                  \
    do {                                                                                        \
        auto const valid{                                                                       \
            validate_serialized_series(record.tick_series.field, TEXT("tick_series." #field))}; \
        if (!valid) {                                                                           \
            return valid;                                                                       \
        }                                                                                       \
    } while (false)
    VALIDATE_SERIES(active_entities);
    VALIDATE_SERIES(spawned_entities);
    VALIDATE_SERIES(destroyed_entities);
    VALIDATE_SERIES(kills);
    VALIDATE_SERIES(active_lasers);
    VALIDATE_SERIES(lasers_fired);
#undef VALIDATE_SERIES
    for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count; ++type) {
        auto const type_name{serialized_name(static_cast<::ioj::sim::EntityType>(type))};
        auto const valid{validate_serialized_series(
            record.tick_series.active_entities_by_type[type],
            FString::Printf(TEXT("tick_series.active_entities_by_type.%s"), *type_name))};
        if (!valid) {
            return valid;
        }
    }
    for (int32 team{}; team < ::ioj::sim::LevelTelemetryTickSeries::team_count; ++team) {
        auto const team_name{serialized_name(ml::to_native(static_cast<ETestTeam>(team)))};
        for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count; ++type) {
            auto const type_name{serialized_name(static_cast<::ioj::sim::EntityType>(type))};
            auto const valid{validate_serialized_series(
                record.tick_series.active_entities_by_team_and_type[team][type],
                FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s.%s"),
                                *team_name,
                                *type_name))};
            if (!valid) {
                return valid;
            }
        }
    }
    for (int32 index{}; index < record.battle_samples.Num(); ++index) {
        auto const& sample{record.battle_samples[index]};
        auto const path{FString::Printf(TEXT("battle_samples[%d]"), index)};
        auto const tick{validate_serialized_integer(
            sample.completed_tick, path + TEXT(".completed_tick"), true)};
        auto const elapsed{validate_serialized_number(
            sample.simulated_elapsed_seconds, path + TEXT(".simulated_elapsed_seconds"), true)};
        auto const alive{validate_serialized_counts(sample.alive, path + TEXT(".alive"))};
        auto const combat{
            validate_serialized_counts(sample.combat.spawned, path + TEXT(".combat.spawned"))};
        auto const active_lasers{
            validate_serialized_integer(sample.active_lasers, path + TEXT(".active_lasers"), true)};
        auto const lasers_fired{
            validate_serialized_integer(sample.lasers_fired, path + TEXT(".lasers_fired"), true)};
        if (!tick || !elapsed || !alive || !combat || !active_lasers || !lasers_fired) {
            return std::unexpected{!tick            ? tick.error()
                                   : !elapsed       ? elapsed.error()
                                   : !alive         ? alive.error()
                                   : !combat        ? combat.error()
                                   : !active_lasers ? active_lasers.error()
                                                    : lasers_fired.error()};
        }
        auto const destroyed{
            validate_serialized_counts(sample.combat.destroyed, path + TEXT(".combat.destroyed"))};
        auto const shots{
            validate_serialized_counts(sample.combat.shots, path + TEXT(".combat.shots"))};
        auto const hits{
            validate_serialized_counts(sample.combat.hits, path + TEXT(".combat.hits"))};
        auto const damage_dealt{validate_serialized_counts(sample.combat.damage_dealt,
                                                           path + TEXT(".combat.damage_dealt"))};
        auto const damage_received{validate_serialized_counts(
            sample.combat.damage_received, path + TEXT(".combat.damage_received"))};
        auto const kills{
            validate_serialized_counts(sample.combat.kills, path + TEXT(".combat.kills"))};
        auto const losses{
            validate_serialized_counts(sample.combat.losses, path + TEXT(".combat.losses"))};
        auto const kill_matrix{validate_serialized_counts(sample.combat.kill_matrix,
                                                          path + TEXT(".combat.kill_matrix"))};
        if (!destroyed || !shots || !hits || !damage_dealt || !damage_received || !kills ||
            !losses || !kill_matrix) {
            return std::unexpected{!destroyed         ? destroyed.error()
                                   : !shots           ? shots.error()
                                   : !hits            ? hits.error()
                                   : !damage_dealt    ? damage_dealt.error()
                                   : !damage_received ? damage_received.error()
                                   : !kills           ? kills.error()
                                   : !losses          ? losses.error()
                                                      : kill_matrix.error()};
        }
    }
    return {};
}
}

auto level_telemetry_runs_directory() -> FString {
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Telemetry"), TEXT("Runs"));
}

auto read_level_telemetry_run(FString const& path)
    -> std::expected<FLevelTelemetryReport, FString> {
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
    -> std::expected<FLevelTelemetryReport, FString> {
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
    if (*schema != 1 && *schema != 2 && *schema != 3 &&
        *schema != FLevelTelemetryReport::schema_version) {
        return std::unexpected{
            FString::Printf(TEXT("Unsupported telemetry schema version %d"), *schema)};
    }

    FLevelTelemetryReport result;
    result.loaded_schema_version = *schema;
#define READ_REQUIRED(destination, expression)            \
    do {                                                  \
        auto parsed_value{expression};                    \
        if (!parsed_value) {                              \
            return std::unexpected{parsed_value.error()}; \
        }                                                 \
        destination = MoveTemp(*parsed_value);            \
    } while (false)

    READ_REQUIRED(result.metadata.run_id,
                  required_native_string(*root, TEXT("run_id"), TEXT("run_id")));
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
                  required_native_string(**level, TEXT("map_name"), TEXT("level.map_name")));
    FString level_id;
    READ_REQUIRED(level_id, required_string(**level, TEXT("level_id"), TEXT("level.level_id")));
    result.metadata.level_id = TCHAR_TO_UTF8(*level_id);
    READ_REQUIRED(
        result.metadata.level_display_name,
        required_native_string(**level, TEXT("display_name"), TEXT("level.display_name")));
    READ_REQUIRED(result.metadata.launched_utc,
                  required_native_string(
                      **timestamps, TEXT("launched_utc"), TEXT("timestamps.launched_utc")));
    READ_REQUIRED(result.completion.completed_utc,
                  required_native_string(
                      **timestamps, TEXT("completed_utc"), TEXT("timestamps.completed_utc")));

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
        READ_REQUIRED(result.metadata.source_sha256,
                      required_native_string(
                          **simulation, TEXT("source_sha256"), TEXT("simulation.source_sha256")));
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
        auto const* duration{(*simulation)->Values.Find(TEXT("requested_duration_seconds"))};
        if (duration && (*duration)->Type != EJson::Null) {
            double value{};
            READ_REQUIRED(value,
                          required_number(**simulation,
                                          TEXT("requested_duration_seconds"),
                                          TEXT("simulation.requested_duration_seconds")));
            if (value < 0.0) {
                return std::unexpected{
                    FString{TEXT("simulation.requested_duration_seconds must be nonnegative")}};
            }
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
    ELevelTelemetryRunEndReason parsed_reason{};
    READ_REQUIRED(
        parsed_reason,
        parse_serialized_enum<ELevelTelemetryRunEndReason>(reason_name, TEXT("completion.reason")));
    result.completion.reason = parsed_reason;
    READ_REQUIRED(result.completion.interrupted,
                  required_bool(**completion, TEXT("interrupted"), TEXT("completion.interrupted")));
    READ_REQUIRED(result.completion.world_end_reason,
                  required_native_string(
                      **completion, TEXT("world_end_reason"), TEXT("completion.world_end_reason")));
    READ_REQUIRED(result.completion.completed_ticks,
                  required_integer<uint64>(
                      **completion, TEXT("completed_ticks"), TEXT("completion.completed_ticks")));
    READ_REQUIRED(result.completion.simulated_elapsed_seconds,
                  required_number(**completion,
                                  TEXT("simulated_elapsed_seconds"),
                                  TEXT("completion.simulated_elapsed_seconds")));
    if (result.completion.simulated_elapsed_seconds < 0.0) {
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
            result.completion.winning_team = ml::to_native(team);
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
        result.completion.mission_mode = ml::to_native(mode);
        result.completion.mission_state = ml::to_native(state);
        result.completion.mission_fail_reason = ml::to_native(fail);
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
    PARSE_SERIES(active_lasers);
    PARSE_SERIES(lasers_fired);
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
    VALIDATE_COUNT(active_lasers);
    VALIDATE_COUNT(lasers_fired);
#undef VALIDATE_COUNT
    auto const by_type{required_object(**tick_series,
                                       TEXT("active_entities_by_type"),
                                       TEXT("tick_series.active_entities_by_type"))};
    auto const by_team{required_object(**tick_series,
                                       TEXT("active_entities_by_team_and_type"),
                                       TEXT("tick_series.active_entities_by_team_and_type"))};
    if (!by_type || !by_team) {
        return std::unexpected{!by_type ? by_type.error() : by_team.error()};
    }
    for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count; ++type) {
        auto const type_name{serialized_name(static_cast<::ioj::sim::EntityType>(type))};
        auto parsed{parse_tick_series(
            **by_type,
            *type_name,
            FString::Printf(TEXT("tick_series.active_entities_by_type.%s"), *type_name),
            result.tick_series.active_entities_by_type[type])};
        if (!parsed) {
            return std::unexpected{parsed.error()};
        }
        auto valid{validate_nonnegative_series(
            result.tick_series.active_entities_by_type[type],
            FString::Printf(TEXT("tick_series.active_entities_by_type.%s"), *type_name))};
        if (!valid) {
            return std::unexpected{valid.error()};
        }
    }
    for (int32 team{}; team < ::ioj::sim::LevelTelemetryTickSeries::team_count; ++team) {
        auto const team_name{serialized_name(ml::to_native(static_cast<ETestTeam>(team)))};
        auto const team_object{required_object(
            **by_team,
            *team_name,
            FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s"), *team_name))};
        if (!team_object) {
            return std::unexpected{team_object.error()};
        }
        for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count; ++type) {
            auto const type_name{serialized_name(static_cast<::ioj::sim::EntityType>(type))};
            auto parsed{parse_tick_series(
                **team_object,
                *type_name,
                FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s.%s"),
                                *team_name,
                                *type_name),
                result.tick_series.active_entities_by_team_and_type[team][type])};
            if (!parsed) {
                return std::unexpected{parsed.error()};
            }
            auto valid{validate_nonnegative_series(
                result.tick_series.active_entities_by_team_and_type[team][type],
                FString::Printf(TEXT("tick_series.active_entities_by_team_and_type.%s.%s"),
                                *team_name,
                                *type_name))};
            if (!valid) {
                return std::unexpected{valid.error()};
            }
        }
    }
    if (*schema >= 2) {
        TArray<TSharedPtr<FJsonValue>> const* battle_samples{};
        if (!root->TryGetArrayField(TEXT("battle_samples"), battle_samples) || !battle_samples) {
            return std::unexpected{FString{TEXT("battle_samples: required array is missing")}};
        }
        result.battle_samples.Reserve(battle_samples->Num());
        for (int32 index{}; index < battle_samples->Num(); ++index) {
            auto const path{FString::Printf(TEXT("battle_samples[%d]"), index)};
            auto const& value{(*battle_samples)[index]};
            auto const object{value.IsValid() ? value->AsObject() : nullptr};
            if (!object.IsValid()) {
                return std::unexpected{error_at(path, TEXT("sample must be an object"))};
            }
            ::ioj::sim::LevelTelemetryBattleSample sample;
            READ_REQUIRED(sample.completed_tick,
                          required_integer<uint64>(
                              *object, TEXT("completed_tick"), path + TEXT(".completed_tick")));
            READ_REQUIRED(sample.simulated_elapsed_seconds,
                          required_number(*object,
                                          TEXT("simulated_elapsed_seconds"),
                                          path + TEXT(".simulated_elapsed_seconds")));
            if (sample.simulated_elapsed_seconds < 0.0) {
                return std::unexpected{error_at(path + TEXT(".simulated_elapsed_seconds"),
                                                TEXT("value must be nonnegative"))};
            }
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
            auto const active_lasers{optional_integer<int32>(
                *object, TEXT("active_lasers"), path + TEXT(".active_lasers"))};
            if (!active_lasers) {
                return std::unexpected{active_lasers.error()};
            }
            if (active_lasers->has_value()) {
                if (**active_lasers < 0) {
                    return std::unexpected{
                        error_at(path + TEXT(".active_lasers"), TEXT("value must be nonnegative"))};
                }
                sample.active_lasers = **active_lasers;
            }
            auto const lasers_fired{optional_integer<int32>(
                *object, TEXT("lasers_fired"), path + TEXT(".lasers_fired"))};
            if (!lasers_fired) {
                return std::unexpected{lasers_fired.error()};
            }
            if (lasers_fired->has_value()) {
                if (**lasers_fired < 0) {
                    return std::unexpected{
                        error_at(path + TEXT(".lasers_fired"), TEXT("value must be nonnegative"))};
                }
                sample.lasers_fired = **lasers_fired;
            }
            result.battle_samples.Add(MoveTemp(sample));
        }
    }
#undef READ_REQUIRED
    return result;
}

auto serialize_level_telemetry_run(FLevelTelemetryReport const& record) -> FString {
    if (!validate_serialized_report(record)) {
        return {};
    }
    auto root{MakeShared<FJsonObject>()};
    root->SetNumberField(TEXT("schema_version"), FLevelTelemetryReport::schema_version);
    root->SetStringField(TEXT("run_id"), UTF8_TO_TCHAR(record.metadata.run_id.c_str()));

    auto level{MakeShared<FJsonObject>()};
    level->SetStringField(TEXT("map_name"), UTF8_TO_TCHAR(record.metadata.map_name.c_str()));
    level->SetStringField(TEXT("level_id"), UTF8_TO_TCHAR(record.metadata.level_id.c_str()));
    level->SetStringField(TEXT("display_name"),
                          UTF8_TO_TCHAR(record.metadata.level_display_name.c_str()));
    root->SetObjectField(TEXT("level"), level);

    auto timestamps{MakeShared<FJsonObject>()};
    timestamps->SetStringField(TEXT("launched_utc"),
                               UTF8_TO_TCHAR(record.metadata.launched_utc.c_str()));
    timestamps->SetStringField(TEXT("completed_utc"),
                               UTF8_TO_TCHAR(record.completion.completed_utc.c_str()));
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
    simulation->SetStringField(TEXT("source_sha256"),
                               UTF8_TO_TCHAR(record.metadata.source_sha256.c_str()));
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
    root->SetObjectField(TEXT("simulation"), simulation);

    auto completion{MakeShared<FJsonObject>()};
    completion->SetStringField(TEXT("reason"), serialized_name(record.completion.reason));
    completion->SetBoolField(TEXT("interrupted"), record.completion.interrupted);
    completion->SetStringField(TEXT("world_end_reason"),
                               UTF8_TO_TCHAR(record.completion.world_end_reason.c_str()));
    if (record.completion.mission_mode.has_value()) {
        completion->SetStringField(TEXT("mission_mode"),
                                   serialized_name(record.completion.mission_mode.value()));
        completion->SetStringField(TEXT("mission_state"),
                                   serialized_name(record.completion.mission_state.value()));
        completion->SetStringField(TEXT("mission_fail_reason"),
                                   serialized_name(record.completion.mission_fail_reason.value()));
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
    if (record.completion.winning_team.has_value()) {
        completion->SetStringField(TEXT("winning_team"),
                                   serialized_name(record.completion.winning_team.value()));
    } else {
        completion->SetField(TEXT("winning_team"), MakeShared<FJsonValueNull>());
    }
    root->SetObjectField(TEXT("completion"), completion);
    root->SetObjectField(TEXT("tick_series"), make_tick_series(record.tick_series));
    root->SetArrayField(TEXT("battle_samples"), make_battle_samples(record.battle_samples));

    FString output;
    auto writer{TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&output)};
    if (!FJsonSerializer::Serialize(root, writer)) {
        return {};
    }
    return output;
}

auto write_level_telemetry_run(FLevelTelemetryReport const& record, FString const& output_directory)
    -> std::expected<FString, FString> {
    auto const valid{validate_serialized_report(record)};
    if (!valid) {
        return std::unexpected{valid.error()};
    }
    auto const json{serialize_level_telemetry_run(record)};
    if (json.IsEmpty()) {
        return std::unexpected{FString{TEXT("JSON serialization failed")}};
    }
    if (!IFileManager::Get().MakeDirectory(*output_directory, true)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not create output directory '%s'"), *output_directory)};
    }

    FString timestamp{UTF8_TO_TCHAR(record.metadata.launched_utc.c_str())};
    timestamp.ReplaceCharInline(TEXT(':'), TEXT('-'));
    FString level_name{UTF8_TO_TCHAR(
        (record.metadata.level_id.empty() ? record.metadata.map_name : record.metadata.level_id)
            .c_str())};
    level_name = FPaths::MakeValidFileName(level_name);
    auto const short_run_id{FString{UTF8_TO_TCHAR(record.metadata.run_id.c_str())}.Left(8)};
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
