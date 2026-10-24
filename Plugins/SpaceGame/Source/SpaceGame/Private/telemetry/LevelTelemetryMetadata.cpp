#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>

#include <SpaceGame/missions/LevelMissionDefinition.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/DateTime.h>
#include <Misc/Guid.h>

auto make_level_telemetry_run_metadata(UWorld const& world,
                                       FLevelMissionDefinition const& mission_definition)
    -> ioj::sim::LevelTelemetryRunMetadata {
    return {
        .run_id = TCHAR_TO_UTF8(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),
        .map_name = TCHAR_TO_UTF8(*UGameplayStatics::GetCurrentLevelName(&world)),
        .level_id = TCHAR_TO_UTF8(*mission_definition.level_id.ToString()),
        .level_display_name = TCHAR_TO_UTF8(*mission_definition.level_display_name),
        .launched_utc = TCHAR_TO_UTF8(*FDateTime::UtcNow().ToIso8601()),
    };
}
