#include "SpaceGame/UnrealMessages.h"

#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <ioj/host/game_host_api.h>
#endif

#include <Misc/AssertionMacros.h>
#include <UObject/WeakObjectPtrTemplates.h>
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <HAL/IConsoleManager.h>
#endif

#include <utility>
#include <vector>

namespace ioj {
namespace {
PlayerSettingsView attached_settings{};
PlayerMenu* attached_menu{};
SystemCapabilities const* attached_capabilities{};
presentation::ProfileView published_profiles{};
std::optional<session::LevelSelectRequest> menu_destination;
std::span<ScreenResolution const> window_resolutions{};

struct MessageBuffer {
    std::vector<host::ProfileAction> profile_actions;
    std::vector<TWeakObjectPtr<USaveGameViewerWidget>> profile_senders;
    std::vector<host::PreferenceEdit> preference_edits;
    std::vector<TWeakObjectPtr<UOptionsDraft>> preference_senders;
    host::NavigationRequest navigation_request;
    TWeakObjectPtr<ULevelSelectWidget> catalog_request;
    std::optional<levels::LaunchMode> requested_simulation_mode;
};

std::vector<host::WidgetAction> pending_actions;
std::vector<host::WidgetAction> dispatched_actions;
MessageBuffer pending;
MessageBuffer published;

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED && !UE_BUILD_SHIPPING
FAutoConsoleCommand reset_profile_command{
    TEXT("spacegame_reset_test_profile"),
    TEXT("Replaces the dedicated Test Profile and activates it."),
    FConsoleCommandDelegate::CreateLambda(
        [] { queue_profile_action(host::ProfileActionKind::ResetTestProfile, {}, nullptr); })};
#endif
}

auto player_menu() -> PlayerMenu& {
    check(attached_menu);
    return *attached_menu;
}

auto player_settings() -> PlayerSettingsView const& {
    return attached_settings;
}

auto system_capabilities() -> SystemCapabilities const& {
    check(attached_capabilities);
    return *attached_capabilities;
}

auto player_profiles() -> presentation::ProfileView const& {
    return published_profiles;
}

void queue_profile_action(host::ProfileActionKind kind,
                          std::string_view profile_name_or_id,
                          USaveGameViewerWidget* widget) {
    pending.profile_actions.push_back({kind, std::string{profile_name_or_id}});
    pending.profile_senders.emplace_back(widget);
}

void queue_preferences(host::PreferenceEdit edit, UOptionsDraft& draft) {
    pending.preference_edits.push_back(std::move(edit));
    pending.preference_senders.emplace_back(&draft);
}

void request_level_catalog(ULevelSelectWidget& widget) {
    pending.catalog_request = &widget;
}

void request_level(host::LevelSelection level) {
    pending.navigation_request = std::move(level);
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
    return window_resolutions;
}

void hit_menu_control(MenuTarget target) {
    queue_input_action(MenuHit{std::move(target)});
}
void queue_input_action(host::WidgetAction action) {
    pending_actions.push_back(action);
}
void request_simulation_mode(levels::LaunchMode mode) {
    pending.requested_simulation_mode = mode;
}

void clear_requested_simulation_mode() {
    pending.requested_simulation_mode.reset();
}

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
void IojPlayer_Attach(SystemCapabilities const& capabilities, PlayerMenu& menu) {
    attached_menu = &menu;
    attached_capabilities = &capabilities;
}
void host::present_catalog(presentation::LevelCatalogView const& catalog) {
    if (auto* widget{published.catalog_request.Get()}) {
        widget->present_catalog(catalog);
    }
}
void host::present_settings(PlayerSettingsView const& settings,
                            std::span<ScreenResolution const> resolutions) {
    window_resolutions = resolutions;
    attached_settings = settings;
}
void host::present_profiles(presentation::ProfileView const& profiles) {
    published_profiles = profiles;
}

void clear_consumed_messages() {
    published.catalog_request.Reset();
    published.profile_actions.clear();
    published.profile_senders.clear();
    published.preference_edits.clear();
    published.preference_senders.clear();
    published.navigation_request.emplace<std::monostate>();
    published.requested_simulation_mode.reset();
}

auto host::take_widget_actions() -> std::span<WidgetAction const> {
    dispatched_actions.clear();
    dispatched_actions.swap(pending_actions);
    return dispatched_actions;
}

auto host::take_messages() -> UnrealMessages {
    std::swap(pending, published);

    return {published.profile_actions,
            published.preference_edits,
            published.navigation_request,
            published.requested_simulation_mode,
            published.catalog_request.IsValid()};
}

void host::profile_result(std::size_t index, profiles::CreateProfileResult result) {
    if (auto* widget{published.profile_senders[index].Get()}) {
        auto const& action{published.profile_actions[index]};
        widget->profile_action_finished(action.kind, action.profile_name_or_id, result);
    }
}

void host::preference_result(std::size_t index, std::string_view error) {
    if (auto* draft{published.preference_senders[index].Get()}) {
        draft->preference_finished(published.preference_edits[index], error);
    }
}
#endif

void detach_player() {
    pending_actions.clear();
    dispatched_actions.clear();
    published = MessageBuffer{};
    pending = MessageBuffer{};
    menu_destination.reset();
    window_resolutions = {};
    published_profiles = {};
    attached_settings = {};
    attached_capabilities = nullptr;
    attached_menu = nullptr;
}
}
