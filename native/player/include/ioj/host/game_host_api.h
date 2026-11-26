#pragma once

#include <ioj/host/mission_startup.h>
#include <ioj/profiles/create_profile_result.h>
#include <ioj/settings/screen_resolution.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace ioj::host {
struct UnrealMessages;
}

namespace ioj::presentation {
struct MissionCatalogView;
}
namespace ioj::profiles {
class ProfileManager;
class SaveGameBrowser;
}
namespace ioj {
struct SystemCapabilities;
class PlayerSettings;
#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_PLAYER_API __declspec(dllexport)
#else
#define IOJ_PLAYER_API __declspec(dllimport)
#endif
extern "C" {
    IOJ_PLAYER_API void IojPlayer_Attach(PlayerSettings const& settings,
                                         SystemCapabilities const& capabilities,
                                         std::vector<ScreenResolution> const& resolutions,
                                         profiles::ProfileManager const& profiles,
                                         profiles::SaveGameBrowser const& browser);
    IOJ_PLAYER_API auto IojPlayer_MissionStartup() -> MissionStartup;
    IOJ_PLAYER_API void IojPlayer_Detach();
    IOJ_PLAYER_API auto IojPlayer_ApplyPreferences() -> bool;
    IOJ_PLAYER_API auto IojPlayer_GameFailed() -> bool;
    IOJ_PLAYER_API auto IojPlayer_GameReady() -> bool;
    IOJ_PLAYER_API auto IojPlayer_ExitRequested() -> bool;
    IOJ_PLAYER_API void IojPlayer_BeginFrame();
    IOJ_PLAYER_API auto
        IojPlayer_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool;
}
namespace host {
// Call once after the Unreal tick. Views last until the next BeginFrame or detach.
// BeginFrame clears consumed requests and input edges. Axes carry forward.
// Requests queued while handling results belong to the next batch.
IOJ_PLAYER_API void present_catalog(presentation::MissionCatalogView const& catalog);
IOJ_PLAYER_API auto take_messages() -> UnrealMessages;
IOJ_PLAYER_API void profile_result(std::size_t index, profiles::CreateProfileResult result);
IOJ_PLAYER_API void preference_result(std::size_t index, std::string_view error);
}
#undef IOJ_PLAYER_API
}
