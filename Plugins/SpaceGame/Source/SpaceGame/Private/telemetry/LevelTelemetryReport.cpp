#include <SpaceGame/telemetry/LevelTelemetryReport.h>

FLevelTelemetryReport::FLevelTelemetryReport(::ioj::sim::LevelTelemetryRunRecord record)
    : loaded_schema_version{record.loaded_schema_version}
    , completion{MoveTemp(record.completion)}
    , tick_series{MoveTemp(record.tick_series)} {
    battle_samples.Append(record.battle_samples.data(),
                          static_cast<int32>(record.battle_samples.size()));
    static_cast<::ioj::sim::LevelTelemetryRunMetadata&>(metadata) = MoveTemp(record.metadata);
}
