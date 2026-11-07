#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>

#include <ioj/levels/level_metadata.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/DateTime.h>
#include <Misc/Guid.h>

auto make_level_telemetry_run_metadata(UWorld const& world, ioj::levels::LevelMetadata const& level)
    -> ioj::sim::telemetry::RunMetadata {
    return {
        .run_id = TCHAR_TO_UTF8(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),
        .map_name = TCHAR_TO_UTF8(*UGameplayStatics::GetCurrentLevelName(&world)),
        .level_id = level.id.value,
        .level_display_name = level.title,
        .launched_utc = TCHAR_TO_UTF8(*FDateTime::UtcNow().ToIso8601()),
    };
}
