#pragma once

#include <SpaceGame/telemetry/LevelTelemetryRunRecord.h>

#include <expected>

SPACEGAME_API auto serialize_level_telemetry_run(FLevelTelemetryRunRecord const& record) -> FString;
SPACEGAME_API auto deserialize_level_telemetry_run(FString const& json)
    -> std::expected<FLevelTelemetryRunRecord, FString>;
SPACEGAME_API auto read_level_telemetry_run(FString const& path)
    -> std::expected<FLevelTelemetryRunRecord, FString>;
SPACEGAME_API auto level_telemetry_runs_directory() -> FString;
SPACEGAME_API auto write_level_telemetry_run(FLevelTelemetryRunRecord const& record,
                                             FString const& output_directory)
    -> std::expected<FString, FString>;
