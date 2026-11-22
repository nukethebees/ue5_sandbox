#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "EmbeddedHost.h"

#include "GenericPlatform/GenericWindow.h"
#include "SpaceGame/system/GameSubsystem.h"
#include <ioj/levels/authoring/definition_reader.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"

namespace ioj {
namespace {
EmbeddedGameHost* host{};
}
auto embedded_game_host() -> EmbeddedGameHost& {
    check(host);
    return *host;
}

extern "C" __declspec(dllexport) void SandboxHost_Attach(EmbeddedGameHost* application) {
    check(application && !host);
    host = application;
}

extern "C" __declspec(dllexport) bool SandboxHost_GameReady() {
    auto const* viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const* world{viewport ? viewport->GetWorld() : nullptr};
    return world && world->HasBegunPlay();
}

extern "C" __declspec(dllexport) bool SandboxHost_ExitRequested() {
    return IsEngineExitRequested();
}

extern "C" __declspec(dllexport) void SandboxHost_PumpMessages() {
    FPlatformApplicationMisc::PumpMessages(true);
}

extern "C" __declspec(dllexport) void
    SandboxHost_InitializeWindow(int x, int y, int width, int height) {
    auto const window{GEngine->GameViewport->GetWindow().ToSharedRef()};
    auto const native_window{window->GetNativeWindow().ToSharedRef()};
    auto& slate{FSlateApplication::Get()};
    auto const position{window->GetPositionInScreen()};
    auto const size{window->GetSizeInScreen()};
    UE_LOG(LogTemp,
           Display,
           TEXT("Embedded window: Slate (%.0f, %.0f) %.0fx%.0f, host client (%d, %d) %dx%d"),
           position.X,
           position.Y,
           size.X,
           size.Y,
           x,
           y,
           width,
           height);
    slate.OnMovedWindow(native_window, x, y);
    if (size.X != width || size.Y != height) {
        slate.OnSizeChanged(native_window, width, height);
    }

    slate.ProcessApplicationActivationEvent(native_window->IsForegroundWindow());
}

extern "C" __declspec(dllexport) bool SandboxHost_LaunchMission(wchar_t const* path) {
    auto* const world{GEngine->GameViewport->GetWorld()};
    auto* const game{UGameSubsystem::get(world->GetGameInstance())};
    check(game);
    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        auto const message{levels::format_diagnostics(definition.error())};
        UE_LOG(LogTemp,
               Error,
               TEXT("Failed to read mission %s: %s"),
               path,
               UTF8_TO_TCHAR(message.c_str()));
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
