#include "SpaceGame/UnrealMessages.h"

#include "settings/GameHost.h"
#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"
#include "UnrealEndpoint.h"

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include <ioj/host/game_host_api.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/Engine.h>
#include <Engine/GameViewportClient.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <UnrealEngine.h>
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
host::UnrealEndpoint endpoint;
KeyboardLayout keyboard;
PlayerSettingsView attached_settings{};
PlayerMenu* attached_menu{};
SystemCapabilities const* attached_capabilities{};
std::optional<session::LevelSelectRequest> menu_destination;
std::span<ScreenResolution const> window_resolutions{};

struct MessageBuffer {
    std::vector<host::PreferenceEdit> preference_edits;
    std::vector<TWeakObjectPtr<UOptionsDraft>> preference_senders;
    host::NavigationRequest navigation_request;
    TWeakObjectPtr<ULevelSelectWidget> catalog_request;
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
        [] { endpoint.send(host::UnrealMessageKind::ResetTestProfile); })};
#endif
}

auto keyboard_layout() -> KeyboardLayout const& {
    return keyboard;
}
auto menu_presentation() -> MenuPresentation const& {
    return player_menu();
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

void request_profile_refresh() {
    endpoint.send(host::UnrealMessageKind::RefreshProfiles);
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
    endpoint.hit(std::move(target));
}
void request_player_action(host::PlayerAction action) {
    endpoint.player_action(action);
}
void edit_menu_text(MenuControl control, std::string value) {
    endpoint.edit_text({control, std::move(value)});
}
void edit_setting(GameSetting setting, GameSettingValue value) {
    endpoint.edit_setting({setting, std::move(value)});
}
void report_pause_interaction(host::PauseWidgetEvent event) {
    endpoint.pause_interaction(event);
}
void queue_input_action(host::WidgetAction action) {
    pending_actions.push_back(action);
}
void request_simulation_mode(levels::LaunchMode mode) {
    endpoint.request_simulation_mode(mode);
}

#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
namespace {
TWeakObjectPtr<ATestBatchOrchestrator> level_world;
std::optional<host::EngineStatus> reported_status;
}
void attach_level_world(ATestBatchOrchestrator& actor) {
    check(!level_world.IsValid());
    level_world = &actor;
}
void detach_level_world(ATestBatchOrchestrator& actor) {
    if (level_world.Get(true) == &actor) {
        level_world.Reset();
    }
}
void host::UnrealEndpoint::receive(host::NativeMessages const& messages) {
    auto* const viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto* const world{viewport ? viewport->GetWorld() : nullptr};
    auto* const player{world ? Cast<ASpaceGamePlayerController>(world->GetFirstPlayerController())
                             : nullptr};
    for (auto const [kind, index] : messages.order) {
        using enum NativeMessageKind;
        switch (kind) {
            case Key:
                host::present_input(messages.keys[index]);
                break;
            case Text:
                host::present_input(messages.texts[index]);
                break;
            case Pointer:
                host::present_input(messages.pointers[index]);
                break;
            case Pause:
                pause_presented(host::present_pause(messages.pause_commands[index]));
                break;
            case Viewer: {
                auto const& step{messages.viewer_steps[index]};
                player->present_viewer_motion(step.motion, step.seconds);
                break;
            }
            case Steering: {
                TOptional<FVector2D> position;
                if (auto direction{messages.steering[index]}) {
                    position.Emplace(direction->x(), direction->y());
                }
                player->present_steering(position);
                break;
            }
            case OpenMenu:
                menu_selection().emplace(levels::LevelId{player_menu().selected_level},
                                         messages.errors[index]);
                level_world.Reset();
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/MainMenu"));
                break;
            case OpenLevel:
                level_world.Reset();
                UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/GameRuntime"));
                break;
            case InitializeLevel: {
                auto const& init{messages.initializations[index]};
                auto result{level_world->initialize_level(init.level, init.frame)};
                std::string error;
                if (!result) {
                    auto const message{
                        result.error().format(to_fstring(init.level.setup.source_path))};
                    UE_LOG(LogTemp, Error, TEXT("%s"), *message);
                    level_world->release_level();
                    error = to_string(message);
                }
                level_initialized(std::move(error));
                break;
            }
            case Frame: {
                auto const& frame{messages.frames[index]};
                level_world->present_level(frame.seconds, frame.frame);
                break;
            }
            case SimulationPaused:
                level_world->on_simulation_paused.Broadcast();
                break;
            case Finish: {
                auto const& result{messages.completions[index]};
                if (level_world.IsValid() && level_world->has_level()) {
                    level_world->finish_level(
                        result.level, result.scored, result.new_best, result.show_results);
                }
                break;
            }
            case Profiles:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->save_game_viewer()}) {
                        widget->present_profiles(messages.profiles[index]);
                    }
                }
                break;
            case ProfileResult:
                if (auto* main{player ? player->main_menu() : nullptr}) {
                    if (auto* widget{main->save_game_viewer()}) {
                        widget->show_create_result(messages.profile_results[index]);
                    }
                }
                break;
            case Keyboard:
                keyboard = *messages.keyboards[index];
                break;
            case Benchmark:
                host::select_benchmark_button(messages.benchmark_selections[index] != 0);
                break;
            default:
                checkNoEntry();
        }
    }
    host::EngineStatus const current_status{world && world->HasBegunPlay(),
                                            level_world.IsValid(),
                                            level_world.IsValid() && level_world->has_level(),
                                            IsEngineExitRequested(),
                                            !!GIsCriticalError};
    if (current_status != reported_status) {
        reported_status = current_status;
        engine_status(current_status);
    }
}
void IojPlayer_Exchange(host::NativeMessages const& outgoing, host::UnrealMessages& incoming) {
    endpoint.receive(outgoing);
    incoming = endpoint.messages();
}
void IojPlayer_ReleaseMessages() {
    endpoint.clear();
}
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
void clear_consumed_messages() {
    published.catalog_request.Reset();
    published.preference_edits.clear();
    published.preference_senders.clear();
    published.navigation_request.emplace<std::monostate>();
}

auto host::take_widget_actions() -> std::span<WidgetAction const> {
    dispatched_actions.clear();
    dispatched_actions.swap(pending_actions);
    return dispatched_actions;
}

auto host::take_messages() -> HostRequests {
    std::swap(pending, published);

    return {published.preference_edits,
            published.navigation_request,
            published.catalog_request.IsValid()};
}

void host::preference_result(std::size_t index, std::string_view error) {
    if (auto* draft{published.preference_senders[index].Get()}) {
        draft->preference_finished(published.preference_edits[index], error);
    }
}
#endif

void detach_player() {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    reported_status.reset();
    level_world.Reset();
#endif
    endpoint.clear();
    pending_actions.clear();
    dispatched_actions.clear();
    published = MessageBuffer{};
    pending = MessageBuffer{};
    menu_destination.reset();
    window_resolutions = {};
    attached_settings = {};
    attached_capabilities = nullptr;
    attached_menu = nullptr;
}
}
