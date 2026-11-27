#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"

#include "SpaceGame/simulation/TestBatchOrchestrator.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/host/game_host_api.h>
#include <ioj/host/level_host.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/launch_options.h>
#include <ioj/settings/player_settings_view.h>
#include <ioj/settings/screen_resolution.h>
#include <SpaceGame/settings/GameVideoSettings.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>

#include <SandboxCoreEngine/strings.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/Platform.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Slate/SceneViewport.h"
#include "UnrealEngine.h"
#include "Widgets/SWindow.h"
#include <Engine/GameInstance.h>

#include <cstdint>
#include <string>
#include <utility>

namespace ioj {
namespace host {
namespace {
auto controller() -> ASpaceGamePlayerController* {
    auto* const world{GEngine->GameViewport->GetWorld()};
    return world ? Cast<ASpaceGamePlayerController>(world->GetFirstPlayerController()) : nullptr;
}
}
auto present_pause(PauseCommand command) -> bool {
    auto* player{controller()};
    check(player);
    return player->present_pause(command);
}
void select_benchmark_button(bool end_selected) {
    auto* player{controller()};
    check(player);
    if (auto* hud{player->get_benchmark_hud()}) {
        hud->select_button(end_selected);
    }
}
void present_menu(MenuUpdate const& update) {
    if (auto* player{controller()}) {
        player->present_menu(update);
    }
}
}
void IojPlayer_Detach() {
    detach_player();
}

auto IojPlayer_ApplyPreferences() -> bool {
    auto const& values{player_settings().applied_values};
    if (auto result{FGameVideoSettings{}.apply_quality(values.video)}; !result) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Applying video preferences failed: %s"),
               *to_fstring(result.error()));
        return false;
    }

    auto* audio{GEngine->GameViewport->GetWorld()->GetGameInstance()->GetSubsystem<UGameAudio>()};
    check(audio);
    audio->set_master_volume(values.audio.master_volume);
    audio->set_music_volume(values.audio.music_volume);
    audio->set_sfx_volume(values.audio.sfx_volume);
    audio->set_ui_volume(values.audio.ui_volume);
    return true;
}

auto IojPlayer_GameFailed() -> bool {
    return GIsCriticalError;
}

auto IojPlayer_GameReady() -> bool {
    auto const* viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const* world{viewport ? viewport->GetWorld() : nullptr};
    return world && world->HasBegunPlay();
}

auto IojPlayer_ExitRequested() -> bool {
    return IsEngineExitRequested();
}

void IojPlayer_BeginFrame() {
    clear_consumed_messages();
}

auto IojPlayer_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool {
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

}
#endif
