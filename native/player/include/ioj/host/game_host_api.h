#pragma once

#include <ioj/host/messages.h>

namespace ioj {
#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_PLAYER_API __declspec(dllexport)
#else
#define IOJ_PLAYER_API __declspec(dllimport)
#endif
extern "C" {
    // The reply borrows Unreal's outbox until the next exchange, engine tick or release.
    IOJ_PLAYER_API void IojPlayer_Exchange(host::NativeMessages const& outgoing,
                                           host::UnrealMessages& incoming);
    IOJ_PLAYER_API void IojPlayer_ReleaseMessages();
    IOJ_PLAYER_API void IojPlayer_Detach();
    IOJ_PLAYER_API void IojPlayer_ReportEngineResult();
    IOJ_PLAYER_API auto
        IojPlayer_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool;
}
#undef IOJ_PLAYER_API
}
