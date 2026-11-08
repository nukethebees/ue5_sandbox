#include "SpaceGame/persistence/SpaceSaveSubsystem.h"

#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/sample_profile_records.h>
#include <ioj/profiles/save_browser.h>
#include <SandboxGameShared/core/SandboxDeveloperSettings.h>

#include <SandboxCoreEngine/strings.h>

#include <CoreGlobals.h>
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <HAL/IConsoleManager.h>
#include <HAL/PlatformProcess.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/Guid.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>

namespace space_save_subsystem {
void reset_test_profile_command(TArray<FString> const&, UWorld* const world) {
    if (!IsValid(world)) {
        UE_LOG(
            LogSandboxSubsystem, Warning, TEXT("spacegame_reset_test_profile: World is invalid."));
        return;
    }

    auto* const game_instance{world->GetGameInstance()};
    auto* const subsystem{ioj::USpaceSaveSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandboxSubsystem,
               Warning,
               TEXT("spacegame_reset_test_profile: Save subsystem is invalid."));
        return;
    }

    if (subsystem->reset_test_profile()) {
        UE_LOG(LogSandboxSubsystem,
               Display,
               TEXT("Reset and activated the deterministic Test Profile."));
    }
}

#if !UE_BUILD_SHIPPING
FAutoConsoleCommandWithWorldAndArgs reset_test_profile_console_command{
    TEXT("spacegame_reset_test_profile"),
    TEXT("Replaces the dedicated Test Profile with deterministic outcomes and activates it."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&reset_test_profile_command)};
#endif
}

namespace ioj {
auto USpaceSaveSubsystem::get(UGameInstance const* game_instance) -> USpaceSaveSubsystem* {
    return IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr;
}
void USpaceSaveSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    profile_manager_ = profiles::ProfileManager{make_storage()};
    if (!profile_manager_.initialise()) {
        UE_LOG(LogSandboxSubsystem, Error, TEXT("Failed to initialize native save profiles."));
    }
    log_save_data();
}
void USpaceSaveSubsystem::Deinitialize() {
    log_save_data();
    Super::Deinitialize();
}
auto USpaceSaveSubsystem::get_active_profile_id() const -> std::string const& {
    return profile_manager_.active_profile_id();
}
auto USpaceSaveSubsystem::get_level_progress(FLevelId level_id) const
    -> profiles::LevelProgressSummary {
    return profiles::summarize_level_progress(
        levels::LevelId{TCHAR_TO_UTF8(*level_id.value.ToString())},
        profile_manager_.active_records());
}
auto USpaceSaveSubsystem::is_level_completed(FLevelId level_id) const -> bool {
    return get_level_progress(level_id).state() == profiles::LevelProgressState::Completed;
}
auto USpaceSaveSubsystem::has_active_profile() const -> bool {
    return profile_manager_.has_active_profile();
}
auto USpaceSaveSubsystem::unlock_all_missions() const -> bool {
    return profile_manager_.unlock_all_missions();
}
auto USpaceSaveSubsystem::start_levels_paused() const -> bool {
    return profile_manager_.start_levels_paused();
}
auto USpaceSaveSubsystem::create_profile(FString const& name) -> profiles::CreateProfileResponse {
    return profile_manager_.create_profile(TCHAR_TO_UTF8(*name));
}
bool USpaceSaveSubsystem::activate_profile(std::string const& id) {
    return profile_manager_.activate_profile(id);
}
bool USpaceSaveSubsystem::reset_test_profile() {
#if UE_BUILD_SHIPPING
    return false;
#else
    return profile_manager_.reset_test_profile(profiles::sample_profile_records());
#endif
}
bool USpaceSaveSubsystem::set_unlock_all_missions(bool enabled) {
    return profile_manager_.set_unlock_all_missions(enabled);
}
bool USpaceSaveSubsystem::set_start_levels_paused(bool enabled) {
    return profile_manager_.set_start_levels_paused(enabled);
}
void USpaceSaveSubsystem::log_save_data() const {
#if WITH_EDITOR
    if (!GetDefault<USandboxDeveloperSettings>()->print_save_data) {
        return;
    }
#endif
    UE_LOG(LogSandboxSubsystem,
           Display,
           TEXT("Native save profile %s: %llu profiles, %llu outcomes"),
           *to_fstring(profile_manager_.active_profile_id()),
           static_cast<unsigned long long>(profile_manager_.profiles().size()),
           static_cast<unsigned long long>(profile_manager_.active_records().size()));
}
auto USpaceSaveSubsystem::make_storage() -> profiles::ProfileStorage {
    auto const local_root{FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"))};
    if (GIsAutomationTesting) {
        auto const root{FPaths::Combine(FPaths::ProjectSavedDir(),
                                        TEXT("Automation/SaveProfiles"),
                                        FGuid::NewGuid().ToString(EGuidFormats::Digits))};
        return profiles::make_file_profile_storage(std::filesystem::path{*root});
    }
    auto const application_root{FPlatformProcess::GetApplicationSettingsDir(
        {FPlatformProcess::ApplicationSettingsContext::Context::LocalUser, false})};
    auto const development_root{
        FPaths::Combine(application_root, TEXT("IsleOfJebbers/DevelopmentSaves"))};
    profiles::StorageOptions options{
#if WITH_EDITOR
        .editor = true,
#endif
        .unattended = FApp::IsUnattended(),
        .local = FParse::Param(FCommandLine::Get(), TEXT("SpaceSaveLocal")),
        .shared = FParse::Param(FCommandLine::Get(), TEXT("SpaceSaveShared")),
        .clone_shared = FParse::Param(FCommandLine::Get(), TEXT("SpaceSaveCloneShared"))};
    FString experiment;
    if (FParse::Value(FCommandLine::Get(), TEXT("SpaceSaveExperiment="), experiment, false)) {
        options.experiment = TCHAR_TO_UTF8(*experiment);
    }
    auto const selection{profiles::resolve_profile_storage(
        options, std::filesystem::path{*development_root}, std::filesystem::path{*local_root})};
    if (!selection) {
        UE_LOG(LogSandboxSubsystem,
               Error,
               TEXT("Invalid native save storage selection: %s"),
               *to_fstring(selection.error()));
        return {};
    }
    if (selection->clone_shared) {
        std::string error;
        if (!profiles::clone_profile_files(
                std::filesystem::path{*development_root} / "Shared", selection->root, error)) {
            UE_LOG(LogSandboxSubsystem,
                   Error,
                   TEXT("Failed to clone native profiles: %s"),
                   *to_fstring(error));
            return {};
        }
    }
    return profiles::make_file_profile_storage(selection->root);
}

} // namespace ioj
