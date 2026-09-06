#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

auto LexToString(ELevelTelemetryRunEndReason const value) -> TCHAR const* {
    switch (value) {
        case ELevelTelemetryRunEndReason::MissionSucceeded:
            return TEXT("mission_succeeded");
        case ELevelTelemetryRunEndReason::MissionFailed:
            return TEXT("mission_failed");
        case ELevelTelemetryRunEndReason::OrchestratorReset:
            return TEXT("orchestrator_reset");
        case ELevelTelemetryRunEndReason::WorldEnd:
            return TEXT("world_end");
    }

    checkNoEntry();
    return TEXT("unknown");
}

namespace {
auto team_name(int32 const index) -> TCHAR const* {
    switch (static_cast<ETestTeam>(index)) {
        case ETestTeam::White:
            return TEXT("white");
        case ETestTeam::Red:
            return TEXT("red");
        case ETestTeam::Green:
            return TEXT("green");
        case ETestTeam::Blue:
            return TEXT("blue");
        case ETestTeam::Orange:
            return TEXT("orange");
        case ETestTeam::Yellow:
            return TEXT("yellow");
        case ETestTeam::COUNT:
            break;
    }
    return TEXT("unknown");
}

auto entity_type_name(int32 const index) -> TCHAR const* {
    switch (static_cast<ETestEntityType>(index)) {
        case ETestEntityType::PlayerShip:
            return TEXT("player_ship");
        case ETestEntityType::Turret:
            return TEXT("turret");
        case ETestEntityType::CapitalShip:
            return TEXT("capital_ship");
        case ETestEntityType::CapitalShipFighter:
            return TEXT("capital_ship_fighter");
        case ETestEntityType::TubeSpinner:
            return TEXT("tube_spinner");
        case ETestEntityType::COUNT:
            break;
    }
    return TEXT("unknown");
}

auto mission_state_name(ETestMissionState const value) -> TCHAR const* {
    switch (value) {
        case ETestMissionState::NotStarted:
            return TEXT("not_started");
        case ETestMissionState::Running:
            return TEXT("running");
        case ETestMissionState::Succeeded:
            return TEXT("succeeded");
        case ETestMissionState::Failed:
            return TEXT("failed");
        case ETestMissionState::Disabled:
            return TEXT("disabled");
    }
    return TEXT("unknown");
}

auto mission_mode_name(ETestMissionMode const value) -> TCHAR const* {
    switch (value) {
        case ETestMissionMode::None:
            return TEXT("none");
        case ETestMissionMode::SurviveTime:
            return TEXT("survive_time");
        case ETestMissionMode::KillEnemies:
            return TEXT("kill_enemies");
        case ETestMissionMode::KillEnemiesWithinTime:
            return TEXT("kill_enemies_within_time");
    }
    return TEXT("unknown");
}

auto mission_fail_reason_name(ETestMissionFailReason const value) -> TCHAR const* {
    switch (value) {
        case ETestMissionFailReason::None:
            return TEXT("none");
        case ETestMissionFailReason::PlayerKilled:
            return TEXT("player_killed");
        case ETestMissionFailReason::TimeElapsed:
            return TEXT("time_elapsed");
        case ETestMissionFailReason::DefenceObjectiveFailed:
            return TEXT("defence_objective_failed");
    }
    return TEXT("unknown");
}

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
    ticks.Reserve(count);
    values.Reserve(count);
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
    elapsed_seconds.Reserve(count);
    completed_ticks.Reserve(count);
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
            entity_type_name(entity_type_index),
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
                entity_type_name(entity_type_index),
                make_series(
                    source.active_entities_by_team_and_type[team_index][entity_type_index]));
        }
        active_by_team_and_type->SetObjectField(team_name(team_index), team);
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
    root->SetObjectField(TEXT("simulation"), simulation);

    auto completion{MakeShared<FJsonObject>()};
    completion->SetStringField(TEXT("reason"), LexToString(record.completion.reason));
    completion->SetBoolField(TEXT("interrupted"), record.completion.interrupted);
    completion->SetStringField(TEXT("world_end_reason"), record.completion.world_end_reason);
    if (record.completion.mission_mode.IsSet()) {
        completion->SetStringField(TEXT("mission_mode"),
                                   mission_mode_name(record.completion.mission_mode.GetValue()));
        completion->SetStringField(TEXT("mission_state"),
                                   mission_state_name(record.completion.mission_state.GetValue()));
        completion->SetStringField(
            TEXT("mission_fail_reason"),
            mission_fail_reason_name(record.completion.mission_fail_reason.GetValue()));
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
    root->SetObjectField(TEXT("completion"), completion);
    root->SetObjectField(TEXT("completed_ticks_by_real_time"),
                         make_realtime_series(record.completed_ticks_by_real_time));
    root->SetObjectField(TEXT("tick_series"), make_tick_series(record.tick_series));

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
