#include "SpaceGame/system/GameSubsystem.h"

#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>
#include "SpaceGameSimulation/memory/GameMemoryBootstrap.h"

#include "persistence/ExistingSaveGameBrowserSource.h"

#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiSettings.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <HAL/PlatformMemory.h>
#include <HAL/PlatformMisc.h>
#include <HAL/PlatformProperties.h>
#include <Kismet/GameplayStatics.h>
#include <Subsystems/SubsystemCollection.h>

#if WITH_CPU_FEATURES
#include <cpuinfo_x86.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/game_capabilities_windows.h"
#endif

namespace ml::ioj {
namespace level_launch {
auto is_valid_time_scale(double const value) noexcept -> bool {
    return FMath::IsFinite(value) && value > 0.0 && value <= maximum_time_scale;
}
}

namespace {
auto query_platform_capabilities() -> FGameCapabilities {
    FGameCapabilities capabilities;
    capabilities.platform_name = ANSI_TO_TCHAR(FPlatformProperties::PlatformName());
    capabilities.host_architecture = FPlatformMisc::GetHostArchitecture();
    FPlatformMisc::GetOSVersions(capabilities.operating_system_version,
                                 capabilities.operating_system_subversion);
    capabilities.cpu_vendor = FPlatformMisc::GetCPUVendor();
    capabilities.cpu_brand = FPlatformMisc::GetCPUBrand();
    capabilities.primary_gpu_brand = FPlatformMisc::GetPrimaryGPUBrand();
    capabilities.physical_core_count = FPlatformMisc::NumberOfCores();
    capabilities.logical_core_count = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    capabilities.total_physical_memory_bytes = FPlatformMemory::GetConstants().TotalPhysical;

#if WITH_CPU_FEATURES
    auto const features{cpu_features::GetX86Info().features};
    capabilities.cpu_simd = FCpuSimdCapabilities{.available = true,
                                                 .sse = features.sse != 0,
                                                 .sse2 = features.sse2 != 0,
                                                 .sse3 = features.sse3 != 0,
                                                 .ssse3 = features.ssse3 != 0,
                                                 .sse4_1 = features.sse4_1 != 0,
                                                 .sse4_2 = features.sse4_2 != 0,
                                                 .sse4a = features.sse4a != 0,
                                                 .avx = features.avx != 0,
                                                 .avx2 = features.avx2 != 0,
                                                 .avx_vnni = features.avx_vnni != 0,
                                                 .avx512_f = features.avx512f != 0,
                                                 .avx512_cd = features.avx512cd != 0,
                                                 .avx512_bw = features.avx512bw != 0,
                                                 .avx512_dq = features.avx512dq != 0,
                                                 .avx512_vl = features.avx512vl != 0,
                                                 .amx_tile = features.amx_tile != 0,
                                                 .amx_bf16 = features.amx_bf16 != 0,
                                                 .amx_int8 = features.amx_int8 != 0,
                                                 .amx_fp16 = features.amx_fp16 != 0};
#endif

#if PLATFORM_WINDOWS
    capabilities.windows = detail::query_windows_platform_capabilities();
#endif

    return capabilities;
}
}

/* **************************************** */
// Lifecycle and services
/* **************************************** */
auto UGameSubsystem::get(UGameInstance const* const game_instance) -> UGameSubsystem* {
    return IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr;
}
void UGameSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    game_memory_ = FGameMemoryBootstrap::create_game_memory();

    audio_.initialize(*GetGameInstance());

    collection.InitializeDependency(UGameSettingsSubsystem::StaticClass());
    auto* const settings_subsystem{GetGameInstance()->GetSubsystem<UGameSettingsSubsystem>()};
    if (IsValid(settings_subsystem)) {
        settings_subsystem->settings_changed.AddUObject(this, &ThisClass::update_audio_settings);
        update_audio_settings();
    } else {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("UGameSubsystem::Initialize: Game settings subsystem is invalid; audio "
                    "will use its default volume."));
    }
    collection.InitializeDependency(UGameUiStyleSubsystem::StaticClass());

    collection.InitializeDependency(USpaceSaveSubsystem::StaticClass());
    auto* const save_subsystem{USpaceSaveSubsystem::get(GetGameInstance())};
    if (!IsValid(save_subsystem)) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("UGameSubsystem::Initialize: Space save subsystem is invalid."));
        return;
    }

    save_game_browser_ = FSaveGameBrowser{
        [save_subsystem] { return detail::discover_existing_save_profiles(*save_subsystem); },
        [save_subsystem](FString const& profile_id) {
            return detail::load_existing_save_profile(*save_subsystem, profile_id);
        }};

    platform_capabilities_ = query_platform_capabilities();

    save_game_browser_.refresh();
}

void UGameSubsystem::Deinitialize() {
    auto* const game_instance{GetGameInstance()};
    auto* const settings_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameSettingsSubsystem>() : nullptr};
    if (IsValid(settings_subsystem)) {
        settings_subsystem->settings_changed.RemoveAll(this);
    }
    audio_.stop_menu_ambience();
    audio_.stop_player_ship_ambience();
    audio_.stop_button_audio();

    game_memory_.Reset();
    Super::Deinitialize();
}

auto UGameSubsystem::get_game_memory() noexcept -> FGameMemory& {
    check(game_memory_.IsValid());
    return *game_memory_;
}

auto UGameSubsystem::get_platform_capabilities() const -> FGameCapabilities const& {
    return platform_capabilities_;
}

auto UGameSubsystem::get_save_game_browser() -> FSaveGameBrowser& {
    return save_game_browser_;
}

auto UGameSubsystem::get_audio() -> FGameAudioFacade {
    return audio_.facade();
}

void UGameSubsystem::start_menu_ambience() {
    audio_.start_menu_ambience();
}

void UGameSubsystem::stop_menu_ambience() {
    audio_.stop_menu_ambience();
}

void UGameSubsystem::start_player_ship_ambience() {
    audio_.start_player_ship_ambience();
}

void UGameSubsystem::stop_player_ship_ambience() {
    audio_.stop_player_ship_ambience();
}

void UGameSubsystem::update_audio_settings() {
    auto* const game_instance{GetGameInstance()};
    auto* const settings_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameSettingsSubsystem>() : nullptr};
    if (!IsValid(settings_subsystem)) {
        return;
    }
    auto const& settings{settings_subsystem->settings_state()};
    audio_.set_music_volume(settings.music_volume);
    audio_.set_sfx_volume(settings.sfx_volume);
}

/* **************************************** */
// UI configuration
/* **************************************** */
auto UGameSubsystem::get_ui_style() const -> FGameUiStyle const& {
    return GetGameInstance()->GetSubsystem<UGameUiStyleSubsystem>()->get_ui_style();
}

auto UGameSubsystem::set_ui_theme(USpaceGameUiTheme* const theme) -> bool {
    return GetGameInstance()->GetSubsystem<UGameUiStyleSubsystem>()->set_ui_theme(theme);
}

/* **************************************** */
// Level launch and navigation
/* **************************************** */
void UGameSubsystem::set_pending_level(FLevelDefinition definition,
                                       FString source_path,
                                       FString source_sha256,
                                       FLevelLaunchOptions options) {
    pending_level_.Emplace(FPendingLevelDefinition{.definition = MoveTemp(definition),
                                                   .source_path = MoveTemp(source_path),
                                                   .source_sha256 = MoveTemp(source_sha256),
                                                   .options = MoveTemp(options)});
    level_launch_error_.Reset();
}

auto UGameSubsystem::take_pending_level() -> TOptional<FPendingLevelDefinition> {
    auto pending{MoveTemp(pending_level_)};
    pending_level_.Reset();
    return pending;
}

auto UGameSubsystem::return_to_level_select(FName const preferred_level_id) -> bool {
    if (level_transition_in_progress_) {
        return false;
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("UGameSubsystem::return_to_level_select: World is invalid."));
        return false;
    }

    pending_level_.Reset();
    level_select_request_.Emplace(FLevelSelectRequest{.preferred_level_id = preferred_level_id});
    level_transition_in_progress_ = true;
    UGameplayStatics::OpenLevel(world, get_main_menu_level_name());
    return true;
}

auto UGameSubsystem::return_to_telemetry(FString run_id, FString error) -> bool {
    if (level_transition_in_progress_) {
        return false;
    }

    auto* const world{GetWorld()};
    if (!IsValid(world)) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("UGameSubsystem::return_to_telemetry: World is invalid."));
        return false;
    }

    pending_level_.Reset();
    level_select_request_.Emplace(FLevelSelectRequest{
        .destination = EMainMenuDestination::Telemetry,
        .selected_telemetry_run_id = MoveTemp(run_id),
        .telemetry_error = MoveTemp(error),
    });
    level_transition_in_progress_ = true;
    UGameplayStatics::OpenLevel(world, get_main_menu_level_name());
    return true;
}

auto UGameSubsystem::take_level_select_request() -> TOptional<FLevelSelectRequest> {
    auto request{MoveTemp(level_select_request_)};
    level_select_request_.Reset();
    level_transition_in_progress_ = false;
    return request;
}

auto UGameSubsystem::get_main_menu_level_name() -> FName {
    static FName const level_name{TEXT("/SpaceGame/Levels/MainMenu")};
    return level_name;
}

/* **************************************** */
// Launch errors
/* **************************************** */
void UGameSubsystem::set_level_launch_error(FString error) {
    pending_level_.Reset();
    level_launch_error_ = MoveTemp(error);
}

auto UGameSubsystem::has_level_launch_error() const noexcept -> bool {
    return !level_launch_error_.IsEmpty();
}

auto UGameSubsystem::take_level_launch_error() -> FString {
    auto error{MoveTemp(level_launch_error_)};
    level_launch_error_.Reset();
    return error;
}
}
