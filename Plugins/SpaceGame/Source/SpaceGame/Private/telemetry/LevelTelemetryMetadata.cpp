#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>

#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/DateTime.h>
#include <Misc/EngineVersion.h>
#include <Misc/Guid.h>

namespace level_telemetry_metadata {
auto world_type_name(EWorldType::Type const world_type) -> FString {
    switch (world_type) {
        case EWorldType::Game:
            return TEXT("game");
        case EWorldType::Editor:
            return TEXT("editor");
        case EWorldType::PIE:
            return TEXT("pie");
        case EWorldType::EditorPreview:
            return TEXT("editor_preview");
        case EWorldType::GamePreview:
            return TEXT("game_preview");
        case EWorldType::Inactive:
            return TEXT("inactive");
        default:
            return TEXT("other");
    }
}
}

auto make_level_telemetry_environment(UWorld const& world) -> FLevelTelemetryEnvironment {
    FLevelTelemetryEnvironment environment{
        .project_name = FApp::GetProjectName(),
        .engine_version = FEngineVersion::Current().ToString(),
        .build_version = FApp::GetBuildVersion(),
        .build_configuration = LexToString(FApp::GetBuildConfiguration()),
        .execution_mode = FApp::IsGame() ? TEXT("game") : TEXT("editor"),
        .world_type = level_telemetry_metadata::world_type_name(world.WorldType),
    };
    if (GConfig != nullptr) {
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
                           TEXT("ProjectVersion"),
                           environment.project_version,
                           GGameIni);
    }

    auto const* const game_instance{world.GetGameInstance()};
    auto const* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (IsValid(subsystem)) {
        auto const& capabilities{subsystem->get_platform_capabilities()};
        environment.platform = capabilities.platform_name;
        environment.host_architecture = capabilities.host_architecture;
        environment.operating_system_version = capabilities.operating_system_version;
        environment.operating_system_subversion = capabilities.operating_system_subversion;
        environment.cpu_vendor = capabilities.cpu_vendor;
        environment.cpu_brand = capabilities.cpu_brand;
        environment.physical_core_count = capabilities.physical_core_count;
        environment.logical_core_count = capabilities.logical_core_count;
        environment.primary_gpu_brand = capabilities.primary_gpu_brand;
        environment.total_physical_memory_bytes = capabilities.total_physical_memory_bytes;
    } else {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("Telemetry environment metadata is incomplete: game subsystem is unavailable"));
    }

    return environment;
}

auto make_level_telemetry_run_metadata(UWorld const& world,
                                       FLevelMissionDefinition const& mission_definition)
    -> FLevelTelemetryRunMetadata {
    return {
        .run_id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),
        .map_name = UGameplayStatics::GetCurrentLevelName(&world),
        .level_id = mission_definition.level_id,
        .level_display_name = mission_definition.level_display_name,
        .launched_utc = FDateTime::UtcNow().ToIso8601(),
    };
}
