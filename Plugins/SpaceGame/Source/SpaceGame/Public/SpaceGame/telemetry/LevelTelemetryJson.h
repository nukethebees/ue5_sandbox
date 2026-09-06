#pragma once

#include <SpaceGame/telemetry/LevelTelemetryRunRecord.h>

#include <expected>

SPACEGAME_API auto serialize_level_telemetry_run(FLevelTelemetryRunRecord const& record) -> FString;
SPACEGAME_API auto write_level_telemetry_run(FLevelTelemetryRunRecord const& record,
                                             FString const& output_directory)
    -> std::expected<FString, FString>;
