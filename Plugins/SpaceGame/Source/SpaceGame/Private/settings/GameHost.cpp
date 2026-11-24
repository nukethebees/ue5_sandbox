#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"

#include "GenericPlatform/GenericWindow.h"
#include "SandboxHostFingerprint.h"
#include "Slate/SceneViewport.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "UnrealEngine.h"
#include <ioj/host/game_host.h>
#include <ioj/host/game_host_api.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/launch_options.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>

#include <SandboxCoreEngine/strings.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/Platform.h"
#include "HAL/PlatformApplicationMisc.h" // IWYU pragma: keep
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Widgets/SWindow.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace ioj {
namespace {
GameHost* host{};
std::optional<PlayerSettings> prepared_settings;
std::string prepare_error;
}
auto game_host() -> GameHost& {
    check(host);
    return *host;
}

auto SandboxHost_Compatible(std::uint32_t version, std::uint64_t layout, char const* fingerprint)
    -> bool {
    return version == host_interface_version && layout == host_layout_signature() &&
           std::strcmp(fingerprint, SANDBOX_HOST_FINGERPRINT) == 0;
}

auto SandboxHost_Prepare(GameHost* application,
                         wchar_t const* scripts,
                         wchar_t const* personal,
                         ScreenResolution desktop,
                         HostDisplay* saved) -> char const* {
    check(application && !host);
    PlayerSettings settings;
    if (auto result{settings.load(scripts, personal, desktop, application->launch_display)};
        !result) {
        prepare_error = std::move(result.error());
        return prepare_error.c_str();
    }

    *saved = settings.saved_display();
    prepared_settings.emplace(std::move(settings));
    host = application;
    return nullptr;
}

auto take_prepared_settings() -> PlayerSettings {
    check(prepared_settings.has_value());
    return std::move(*std::exchange(prepared_settings, std::nullopt));
}

void SandboxHost_Detach() {
    prepared_settings.reset();
    host = nullptr;
}

auto SandboxHost_GameReady() -> bool {
    auto const* viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const* world{viewport ? viewport->GetWorld() : nullptr};
    return world && world->HasBegunPlay();
}

auto SandboxHost_ExitRequested() -> bool {
    return IsEngineExitRequested();
}

void SandboxHost_PumpMessages() {
    FPlatformApplicationMisc::PumpMessages(true);
}

auto SandboxHost_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool {
    auto const window{GEngine->GameViewport->GetWindow().ToSharedRef()};
    auto const native_window{window->GetNativeWindow().ToSharedRef()};
    auto* viewport{GEngine->GameViewport->GetGameViewport()};
    auto& slate{FSlateApplication::Get()};

    GUELibraryOverrideSettings.WindowWidth = width;
    GUELibraryOverrideSettings.WindowHeight = height;
    GSystemResolution.ResX = width;
    GSystemResolution.ResY = height;

    slate.OnMovedWindow(native_window, x, y);
    slate.OnSizeChanged(native_window, width, height);
    if (viewport && viewport->GetSizeXY() != FIntPoint{width, height}) {
        viewport->SetViewportSize(width, height);
        window->SetViewportSizeDrivenByWindow(true);
    }

    if (activate) {
        slate.ProcessApplicationActivationEvent(native_window->IsForegroundWindow());
    }

    auto const measured{window->GetSizeInScreen()};
    return measured.X == width && measured.Y == height && viewport &&
           viewport->GetSizeXY() == FIntPoint{width, height};
}

auto SandboxHost_LaunchMission(wchar_t const* path) -> bool {
    auto* const world{GEngine->GameViewport->GetWorld()};
    auto* const game{UGameSubsystem::get(world->GetGameInstance())};
    check(game);

    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        auto const message{levels::format_diagnostics(definition.error())};
        UE_LOG(LogTemp, Error, TEXT("Failed to read mission %s: %s"), path, *to_fstring(message));
        return false;
    }

    levels::LaunchOptions options;
    options.control_context =
        definition->camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    game->set_pending_level(std::move(*definition), FString{path}, options);

    UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/GameRuntime"));
    return true;
}
}
#endif
