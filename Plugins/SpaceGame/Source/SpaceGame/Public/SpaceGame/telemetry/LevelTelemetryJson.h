#pragma once

#include <ioj/sim/telemetry/report.h>

#include <CoreMinimal.h>

#include <expected>

SPACEGAME_API auto read_level_telemetry_run(FString const& path)
    -> std::expected<ioj::sim::LevelTelemetryReport, FString>;
SPACEGAME_API auto level_telemetry_runs_directory() -> FString;
SPACEGAME_API auto write_level_telemetry_run(ioj::sim::LevelTelemetryReport const& record,
                                             FString const& output_directory)
    -> std::expected<FString, FString>;
