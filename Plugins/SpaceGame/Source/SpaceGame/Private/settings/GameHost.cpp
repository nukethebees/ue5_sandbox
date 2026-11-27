#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <ioj/host/game_host_api.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "Slate/SceneViewport.h"
#include "UnrealEngine.h"
#include "Widgets/SWindow.h"

namespace ioj {
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
