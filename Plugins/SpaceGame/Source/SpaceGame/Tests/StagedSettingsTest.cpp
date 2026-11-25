#if WITH_DEV_AUTOMATION_TESTS && defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED

#include "settings/GameHost.h"
#include "SpaceGame/settings/GameSettingOption.h"
#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGame/system/GameSubsystem.h"
#include <ioj/levels/authoring/game_configuration.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/window_display_settings.h>

#include "Containers/AnsiString.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h" // IWYU pragma: keep
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include <UObject/StrongObjectPtr.h>

#include <limits>

namespace ioj::staged_settings_tests {
// Start the packaged automation test only in a CTest-owned user directory.
static auto isolated_user_directory() -> bool {
    FString directory;
    return FParse::Value(FCommandLine::Get(), TEXT("UserDir="), directory) && !directory.IsEmpty();
}

static void queue_staged_test() {
    if (isolated_user_directory() &&
        !FPlatformMisc::GetEnvironmentVariable(TEXT("SANDBOX_STAGED_SETTINGS_TEST")).IsEmpty()) {
        GEngine->DeferredCommands.Add(
            TEXT("Automation RunTests Sandbox.Staged.Settings; SoftQuit"));
    }
}

static FDelayedAutoRegisterHelper const start_staged_test{EDelayedRegisterRunPhase::EndOfEngineInit,
                                                          queue_staged_test};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStagedSettings,
                                 "Sandbox.Staged.Settings",
                                 EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)

auto FStagedSettings::RunTest(FString const&) -> bool {
    if (!isolated_user_directory()) {
        AddError(TEXT("The staged settings test requires an isolated -UserDir"));
        return false;
    }
    auto* instance{GEngine->GameViewport->GetGameInstance()};
    TStrongObjectPtr<UOptionsDraft> settings{NewObject<UOptionsDraft>(instance)};
    settings->begin_edit(
        *UGameSubsystem::get(instance), instance->GetFirstGamePlayer(), GameSettingCategory::Video);
    auto const phase{FPlatformMisc::GetEnvironmentVariable(TEXT("SANDBOX_STAGED_SETTINGS_TEST"))};
    if (phase == TEXT("headless")) {
        TestFalse(TEXT("Headless mode is locked"), settings->is_available(GameSetting::WindowMode));
        TestFalse(TEXT("Headless resolution is locked"),
                  settings->is_available(GameSetting::Resolution));
        settings->reset_category(GameSettingCategory::Video);
        settings->apply();
        TestFalse(TEXT("Headless reset needs no preview"),
                  settings->is_awaiting_display_confirmation());
        return true;
    }
    if (phase == TEXT("forced-borderless")) {
        TestTrue(TEXT("Launch override runs borderless"),
                 settings->applied_state().window_mode == GameWindowMode::Borderless);
        TestFalse(TEXT("Launch override locks window mode"),
                  settings->is_available(GameSetting::WindowMode));
        TestFalse(TEXT("Monitor determines borderless resolution"),
                  settings->is_available(GameSetting::Resolution));
        return true;
    }

    if (phase == TEXT("borderless")) {
        TestTrue(TEXT("Exclusive preference runs borderless"),
                 settings->applied_state().window_mode == GameWindowMode::Borderless);
        TestTrue(TEXT("Interactive window mode can change"),
                 settings->is_available(GameSetting::WindowMode));
        TestFalse(TEXT("Borderless uses the monitor size"),
                  settings->is_available(GameSetting::Resolution));
        settings->set_setting(GameSetting::WindowMode, GameWindowMode::Windowed);
        settings->apply();
        settings->confirm_display_changes();
        TestTrue(TEXT("Explicit mode confirmed"),
                 settings->applied_state().window_mode == GameWindowMode::Windowed);
        return true;
    }
    if (phase == TEXT("relaunch")) {
        auto const applied{settings->applied_state()};
        auto const scripts{
            std::filesystem::path{*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())} /
            "LevelScripts"};
        auto const personal{std::filesystem::path{*FPaths::ProjectUserDir()} /
                            "Config/user-settings.scm"};
        auto saved{levels::authoring::read_user_settings(scripts, personal, applied.resolution)};
        TestTrue(TEXT("Confirmed windowed mode survives relaunch"),
                 applied.window_mode == GameWindowMode::Windowed);
        TestTrue(TEXT("Relaunched HWND uses confirmed resolution"),
                 saved && applied.resolution == saved->applied.resolution);
        return true;
    }

    int broadcasts{};
    auto subscription{settings->settings_changed.AddLambda([&broadcasts] { ++broadcasts; })};
    settings->apply();
    settings->settings_changed.Remove(subscription);
    TestEqual(TEXT("Ignored apply sends no notifications"), broadcasts, 0);
    TestFalse(TEXT("Ignored apply starts no ticker"), settings->is_awaiting_display_confirmation());

    auto const initial{settings->applied_state()};
    settings->set_setting(GameSetting::VSync, !initial.vsync);
    settings->apply();
    TestTrue(TEXT("Non-display apply commits"), settings->applied_state().vsync != initial.vsync);
    TestFalse(TEXT("Non-display apply needs no confirmation"),
              settings->is_awaiting_display_confirmation());

    auto const scripts{
        std::filesystem::path{*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())} /
        "LevelScripts"};
    auto const personal{std::filesystem::path{*FPaths::ProjectUserDir()} /
                        "Config/user-settings.scm"};
    auto read_saved = [&] {
        return levels::authoring::read_user_settings(
            scripts, personal, settings->default_state().resolution);
    };
    auto saved{read_saved()};
    if (!TestTrue(TEXT("Read unrelated apply"), saved.has_value())) {
        return false;
    }
    TestTrue(TEXT("Unrelated apply preserves stored exclusive mode"),
             saved->applied.window_mode == GameWindowMode::Fullscreen);
    TestTrue(TEXT("Unrelated apply preserves stored size"),
             saved->applied.resolution == ScreenResolution{800, 600});

    TestTrue(TEXT("Startup mode lock"), !settings->is_available(GameSetting::WindowMode));
    TestTrue(TEXT("Startup client dimensions"), initial.resolution == ScreenResolution{800, 500});
    settings->reset_category(GameSettingCategory::Video);
    TestTrue(TEXT("Reset respects mode lock"),
             settings->settings_state().window_mode == GameWindowMode::Windowed);
    settings->cancel();
    settings->begin_edit(
        *UGameSubsystem::get(instance), instance->GetFirstGamePlayer(), GameSettingCategory::Video);

    auto options{settings->options(GameSetting::Resolution)};
    auto const* alternative{options.FindByPredicate([&](FGameSettingOption const& choice) {
        return std::get<ScreenResolution>(choice.value) != initial.resolution;
    })};
    if (!TestNotNull(TEXT("An alternate display size is available"), alternative)) {
        return false;
    }
    auto const resolution{std::get<ScreenResolution>(alternative->value)};
    settings->set_setting(GameSetting::Resolution, resolution);
    auto failed{
        preview_game_window({GameWindowMode::Windowed,
                             {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}})};
    TestFalse(TEXT("Oversized display preview fails"), failed.has_value());
    TestFalse(TEXT("A rejected preview starts no confirmation"),
              settings->is_awaiting_display_confirmation());
    TestTrue(TEXT("A rejected preview preserves the applied size"),
             settings->applied_state().resolution == initial.resolution);
    saved = read_saved();
    TestTrue(TEXT("A rejected preview preserves the saved size"),
             saved && saved->applied.resolution == ScreenResolution{800, 600});

    settings->apply();
    TestTrue(TEXT("Preview uses the active session"), settings->is_awaiting_display_confirmation());
    settings->set_setting(GameSetting::VSync, initial.vsync);
    settings->apply();
    TestTrue(TEXT("Preview rejects conflicting edits"),
             settings->settings_state().vsync != initial.vsync);
    settings->revert_display_changes();
    TestTrue(TEXT("Revert restores the prior size"),
             settings->settings_state().resolution == initial.resolution);
    settings->set_setting(GameSetting::Resolution, resolution);
    settings->apply();
    settings->confirm_display_changes();
    TestFalse(TEXT("Confirmation clears ticker state"),
              settings->is_awaiting_display_confirmation());
    saved = read_saved();
    TestTrue(TEXT("Confirmation persists the chosen size"),
             saved && saved->applied.resolution == resolution);
    TestTrue(TEXT("Size confirmation preserves stored mode"),
             saved && saved->applied.window_mode == GameWindowMode::Fullscreen);
    auto supported{settings->options(GameSetting::Resolution)};
    auto const* timeout_size{
        supported.FindByPredicate([resolution](FGameSettingOption const& choice) {
            return std::get<ScreenResolution>(choice.value) != resolution;
        })};
    if (!TestNotNull(TEXT("Timeout preview size"), timeout_size)) {
        return false;
    }
    settings->set_setting(GameSetting::Resolution, timeout_size->value);
    settings->apply();
    TestTrue(TEXT("Timeout preview started"), settings->is_awaiting_display_confirmation());
    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
        [this, settings, resolution] {
            TestFalse(TEXT("Timeout closed the preview"),
                      settings->is_awaiting_display_confirmation());
            TestTrue(TEXT("Timeout restored confirmed size"),
                     settings->applied_state().resolution == resolution);
        },
        16.0f));
    return true;
}
}
#endif
