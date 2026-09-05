#include "SpaceGame/system/GameSubsystem.h"

#include "persistence/ExistingSaveGameBrowserSource.h"

#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/style/SpaceGameUiSettings.h"
#include "SpaceGame/ui/style/SpaceGameUiTheme.h"

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <Subsystems/SubsystemCollection.h>

#if PLATFORM_WINDOWS
#include "Windows/game_capabilities_windows.h"
#endif

namespace ml::ioj {
void UGameSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    initialize_ui_style();

    collection.InitializeDependency(USpaceSaveSubsystem::StaticClass());
    auto* const save_subsystem{GetGameInstance()->GetSubsystem<USpaceSaveSubsystem>()};
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

#if PLATFORM_WINDOWS
    platform_capabilities_ = detail::query_windows_platform_capabilities();
#endif

    save_game_browser_.refresh();
}

void UGameSubsystem::initialize_ui_style() {
    auto const* const settings{GetDefault<USpaceGameUiSettings>()};
    auto* theme{settings->default_theme.LoadSynchronous()};
    if (!IsValid(theme)) {
        theme = NewObject<USpaceGameUiTheme>(this, TEXT("default_ui_theme"));
    }

    if (!set_ui_theme(theme)) {
        UE_LOG(LogSandboxUI,
               Fatal,
               TEXT("UGameSubsystem::initialize_ui_style: Failed to compile the UI theme."));
    }
}

auto UGameSubsystem::get_platform_capabilities() const -> FGameCapabilities const& {
    return platform_capabilities_;
}

auto UGameSubsystem::get_save_game_browser() -> FSaveGameBrowser& {
    return save_game_browser_;
}

auto UGameSubsystem::get_ui_style() const -> FGameUiStyle const& {
    return ui_style_;
}

auto UGameSubsystem::get_ui_theme() const -> USpaceGameUiTheme const& {
    check(IsValid(ui_theme_));
    return *ui_theme_;
}

auto UGameSubsystem::set_ui_theme(USpaceGameUiTheme* const theme) -> bool {
    if (!IsValid(theme)) {
        UE_LOG(LogSandboxUI, Error, TEXT("UGameSubsystem::set_ui_theme: Theme is invalid."));
        return false;
    }

    FGameUiStyle compiled;
    if (!theme->compile(compiled)) {
        return false;
    }

    ui_theme_ = theme;
    ui_style_ = MoveTemp(compiled);
    return true;
}

void UGameSubsystem::set_pending_level(FLevelDefinition definition,
                                       FString source_path,
                                       ELevelLaunchMode const launch_mode) {
    pending_level_.Emplace(FPendingLevelDefinition{.definition = MoveTemp(definition),
                                                   .source_path = MoveTemp(source_path),
                                                   .launch_mode = launch_mode});
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
