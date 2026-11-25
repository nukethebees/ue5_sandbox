#include "SpaceGame/UnrealMessages.h"


#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <ioj/host/game_host_api.h>
#endif

#include <Misc/AssertionMacros.h>
#include <UObject/WeakObjectPtrTemplates.h>
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <HAL/IConsoleManager.h>
#endif

#include <memory_resource>
#include <utility>
#include <vector>

namespace ioj {
namespace {
PlayerSettings const* attached_settings{};
GameCapabilities const* attached_capabilities{};
profiles::ProfileManager const* profile_manager{};
profiles::SaveGameBrowser const* attached_browser{};
std::optional<session::LevelSelectRequest> menu_destination;
std::vector<ScreenResolution> const* window_resolutions{};

struct MessageBuffer {
    std::vector<host::ProfileAction> profile_actions;
    std::vector<host::PreferenceEdit> preference_edits;
    host::NavigationRequest navigation_request;
    std::optional<levels::LaunchMode> requested_simulation_mode;
    sim::player::PlayerInputEvents ship_input{*std::pmr::get_default_resource()};
};

MessageBuffer pending;
MessageBuffer published;

}

auto player_settings() -> PlayerSettings const& {
    check(attached_settings);
    return *attached_settings;
}

auto player_capabilities() -> GameCapabilities const& {
    check(attached_capabilities);
    return *attached_capabilities;
}

auto player_profiles() -> profiles::ProfileManager const& {
    check(profile_manager);
    return *profile_manager;
}

auto profile_browser() -> profiles::SaveGameBrowser const& {
    check(attached_browser);
    return *attached_browser;
}

void request_mission(session::PendingLevel mission) {
    pending.navigation_request = std::move(mission);
}

bool request_menu(session::LevelSelectRequest request) {
    if (auto* existing{std::get_if<session::LevelSelectRequest>(&pending.navigation_request)}) {
        if (!request.launch_error.empty()) {
            existing->launch_error = std::move(request.launch_error);
        }
        return false;
    }

    pending.navigation_request = std::move(request);
    return true;
}

auto menu_selection() -> std::optional<session::LevelSelectRequest>& {
    return menu_destination;
}

auto supported_resolutions() -> std::span<ScreenResolution const> {
    return *window_resolutions;
}

void request_simulation_mode(levels::LaunchMode mode) {
    pending.requested_simulation_mode = mode;
}

auto ship_input() -> sim::player::PlayerInputEvents& {
    return pending.ship_input;
}

void reset_level_input() {
    pending.ship_input = sim::player::PlayerInputEvents{*std::pmr::get_default_resource()};
    pending.requested_simulation_mode.reset();
}

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
void IojPlayer_Attach(PlayerSettings const& settings,
                      GameCapabilities const& capabilities,
                      std::vector<ScreenResolution> const& resolutions,
                      profiles::ProfileManager const& profiles,
                      profiles::SaveGameBrowser const& reports) {
    attached_settings = &settings;
    attached_capabilities = &capabilities;
    window_resolutions = &resolutions;
    profile_manager = &profiles;
    attached_browser = &reports;
}

void clear_consumed_messages() {
    published.profile_actions.clear();
    published.preference_edits.clear();
    published.navigation_request.emplace<std::monostate>();
    published.requested_simulation_mode.reset();
}

auto host::take_messages() -> UnrealMessages {
    std::swap(pending, published);
    pending.ship_input.begin_frame(published.ship_input);

    return {published.profile_actions,
            published.preference_edits,
            published.navigation_request,
            published.requested_simulation_mode,
            published.ship_input};
}

#endif

void detach_player() {
    published = MessageBuffer{};
    pending = MessageBuffer{};
    menu_destination.reset();
    window_resolutions = nullptr;
    profile_manager = nullptr;
    attached_browser = nullptr;
    attached_settings = nullptr;
    attached_capabilities = nullptr;
}
}
