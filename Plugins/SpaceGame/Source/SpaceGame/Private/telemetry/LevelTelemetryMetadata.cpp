#include <SpaceGame/telemetry/LevelTelemetryMetadata.h>

#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/DateTime.h>
#include <Misc/EngineVersion.h>
#include <Misc/Guid.h>

namespace level_telemetry_metadata {
auto world_type_name(EWorldType::Type const world_type) -> std::string {
    switch (world_type) {
        case EWorldType::Game:
            return "game";
        case EWorldType::Editor:
            return "editor";
        case EWorldType::PIE:
            return "pie";
        case EWorldType::EditorPreview:
            return "editor_preview";
        case EWorldType::GamePreview:
            return "game_preview";
        case EWorldType::Inactive:
            return "inactive";
        default:
            return "other";
    }
}
}

auto make_level_telemetry_environment(UWorld const& world) -> ioj::sim::LevelTelemetryEnvironment {
    ioj::sim::LevelTelemetryEnvironment environment{
        .project_name = ioj::to_string(FString{FApp::GetProjectName()}),
        .engine_version = ioj::to_string(FString{FEngineVersion::Current().ToString()}),
        .build_version = ioj::to_string(FString{FApp::GetBuildVersion()}),
        .build_configuration = ioj::to_string(FString{LexToString(FApp::GetBuildConfiguration())}),
        .execution_mode = FApp::IsGame() ? "game" : "editor",
        .world_type = level_telemetry_metadata::world_type_name(world.WorldType),
    };
    if (GConfig != nullptr) {
        FString project_version;
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
                           TEXT("ProjectVersion"),
                           project_version,
                           GGameIni);
        environment.project_version = ioj::to_string(project_version);
    }

    auto const* const game_instance{world.GetGameInstance()};
    auto const* const subsystem{ioj::UGameSubsystem::get(game_instance)};
    if (IsValid(subsystem)) {
        auto const& capabilities{subsystem->get_platform_capabilities()};
        environment.platform = ioj::to_string(capabilities.platform_name);
        environment.host_architecture = ioj::to_string(capabilities.host_architecture);
        environment.operating_system_version =
            ioj::to_string(capabilities.operating_system_version);
        environment.operating_system_subversion =
            ioj::to_string(capabilities.operating_system_subversion);
        environment.cpu_vendor = ioj::to_string(capabilities.cpu_vendor);
        environment.cpu_brand = ioj::to_string(capabilities.cpu_brand);
        environment.physical_core_count = capabilities.physical_core_count;
        environment.logical_core_count = capabilities.logical_core_count;
        environment.primary_gpu_brand = ioj::to_string(capabilities.primary_gpu_brand);
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
    -> ioj::sim::LevelTelemetryRunMetadata {
    return {
        .run_id = TCHAR_TO_UTF8(*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),
        .map_name = TCHAR_TO_UTF8(*UGameplayStatics::GetCurrentLevelName(&world)),
        .level_id = TCHAR_TO_UTF8(*mission_definition.level_id.ToString()),
        .level_display_name = TCHAR_TO_UTF8(*mission_definition.level_display_name),
        .launched_utc = TCHAR_TO_UTF8(*FDateTime::UtcNow().ToIso8601()),
    };
}
